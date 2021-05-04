//
// CBLiteCommand.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "CBLiteCommand.hh"

#ifdef _MSC_VER
    #include <Shlwapi.h>
    #pragma comment(lib, "shlwapi.lib")
#else
    #include <fnmatch.h>        // POSIX (?)
#endif

using namespace litecore;
using namespace std;
using namespace fleece;


void CBLiteCommand::writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs) {
    cerr << ansiBold();
    if (!interactive())
        cerr << "cblite ";
    cerr << cmd << ' ' << ansiItalic();
    if (hasFlags)
        cerr << "[FLAGS]" << ' ';
    if (!interactive())
        cerr << "DBPATH ";
    cerr << otherArgs << ansiReset() << "\n";
}


bool CBLiteCommand::processFlag(const std::string &flag,
                                const std::initializer_list<FlagSpec> &specs)
{
    if (CBLiteTool::processFlag(flag, specs))
        return true;
    else if (flag == "--collection") {
        setCollectionName(nextArg("collection name"));
        return true;
    } else {
        return false;
    }
}


void CBLiteCommand::openDatabaseFromNextArg() {
    if (!_db)
        openDatabase(nextArg("database path"), false);
}


void CBLiteCommand::openWriteableDatabaseFromNextArg() {
    if (_db) {
        if (_dbFlags & kC4DB_ReadOnly)
            fail("Database was opened read-only; run `cblite --writeable` to allow writes");
    } else {
        _dbFlags &= ~kC4DB_ReadOnly;
        openDatabaseFromNextArg();
    }
}


C4Collection* CBLiteCommand::collection() {
    if (_collectionName.empty())
        return c4db_getDefaultCollection(_db);
    else
        return c4db_getCollection(_db, slice(_collectionName));
}


void CBLiteCommand::setCollectionName(const std::string &name) {
    _collectionName = name;
    if (_parent)
        _parent->setCollectionName(name);
}


void CBLiteCommand::writeSize(uint64_t n) {
    static const char* kScaleNames[] = {" bytes", "KB", "MB", "GB"};
    int scale = 0;
    double scaled = n;
    while (scaled >= 1024 && scale < 3) {
        scaled /= 1024;
        ++scale;
    }
    auto prec = cout.precision();
    cout.precision(scale < 2 ? 0 : 1);
    cout << fixed << scaled << defaultfloat;
    cout.precision(prec);
    cout << kScaleNames[scale];
}


void CBLiteCommand::getDBSizes(uint64_t &dbSize, uint64_t &blobsSize, uint64_t &nBlobs) {
    dbSize = blobsSize = nBlobs = 0;
    alloc_slice pathSlice = c4db_getPath(_db);
    FilePath path(pathSlice.asString());
    path["db.sqlite3"].forEachMatch([&](const litecore::FilePath &file) {
        dbSize += file.dataSize();
    });
    auto attachmentsPath = path["Attachments/"];
    if (attachmentsPath.exists()) {
        attachmentsPath.forEachFile([&](const litecore::FilePath &file) {
            ++nBlobs;
            blobsSize += file.dataSize();
        });
    }
}


c4::ref<C4Document> CBLiteCommand::readDoc(string docID, C4DocContentLevel content) {
    C4Error error;
    c4::ref<C4Document> doc = c4coll_getDoc(collection(), slice(docID), true, content, &error);
    if (!doc) {
        if (error.domain == LiteCoreDomain && error.code == kC4ErrorNotFound)
            cerr << "Error: Document \"" << docID << "\" not found.\n";
        else
            errorOccurred(format("reading document \"%s\"", docID.c_str()), error);
    }
    return doc;
}


bool CBLiteCommand::canBeUnquotedJSON5Key(slice key) {
    if (key.size == 0 || isdigit(key[0]))
        return false;
    for (unsigned i = 0; i < key.size; i++) {
        if (!isalnum(key[i]) && key[i] != '_' && key[i] != '$')
            return false;
    }
    return true;
}


bool CBLiteCommand::isGlobPattern(string &str) {
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i) {
        char c = str[i];
        if ((c == '*' || c == '?') && (i == 0 || str[i-1] != '\\'))
            return true;
    }
    return false;
}

void CBLiteCommand::unquoteGlobPattern(string &str) {
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i) {
        if (str[i] == '\\') {
            str.erase(i, 1);
            --size;
        }
    }
}


bool CBLiteCommand::globMatch(const char *name, const char *pattern) {
#ifdef _MSC_VER
    return PathMatchSpecA(name, pattern);
#else
    return fnmatch(pattern, name, 0) == 0;
#endif
}


int64_t CBLiteCommand::enumerateDocs(EnumerateDocsOptions options, EnumerateDocsCallback callback) {
    C4Error error;
    C4EnumeratorOptions c4Options = {options.flags};
    c4::ref<C4DocEnumerator> e;
    if (options.collection == nullptr)
        options.collection  = collection();
    if (options.bySequence)
        e = c4coll_enumerateChanges(options.collection, 0, &c4Options, &error);
    else
        e = c4coll_enumerateAllDocs(options.collection, &c4Options, &error);
    if (!e)
        fail("creating enumerator", error);

    int64_t nDocs = 0;
    while (c4enum_next(e, &error)) {
        C4DocumentInfo info;
        c4enum_getDocumentInfo(e, &info);

        if (!options.pattern.empty()) {
            // Check whether docID matches pattern:
            string docID = slice(info.docID).asString();
            if (!globMatch(docID.c_str(), options.pattern.c_str()))
                continue;
        }

        // Handle offset & limit:
        if (options.offset > 0) {
            --options.offset;
            continue;
        }
        if (++nDocs > options.limit && options.limit >= 0) {
            error.code = 0;
            break;
        }

        callback(info, e);
    }
    if (error.code)
        fail("enumerating documents", error);
    return nDocs;
}


void CBLiteCommand::rawPrint(Value body, slice docID, slice revID) {
    alloc_slice jsonBuf = body.toJSON(_json5, true);
    slice restOfJSON = jsonBuf;
    if (docID) {
        // Splice a synthesized "_id" property into the start of the JSON object:
        cout << "{" << ansiDim() << ansiItalic()
        << (_json5 ? "_id" : "\"_id\"") << ":\""
        << ansiReset() << ansiDim()
        << docID << "\"";
        if (revID) {
            cout << "," << ansiItalic()
            << (_json5 ? "_rev" : "\"_rev\"") << ":\""
            << ansiReset() << ansiDim()
            << revID << "\"";
        }
        restOfJSON.moveStart(1);
        if (restOfJSON.size > 1)
            cout << ", ";
        cout << ansiReset();
    }
    cout << restOfJSON;
}


void CBLiteCommand::prettyPrint(Value value,
                                   const string &indent,
                                   slice docID,
                                   slice revID,
                                   const set<alloc_slice> *onlyKeys) {
    // TODO: Support an includeID option
    switch (value.type()) {
        case kFLDict: {
            auto dict = value.asDict();
            string subIndent = indent + "  ";
            int n = 0;
            cout << "{";
            if (docID) {
                n++;
                cout << '\n' << subIndent << ansiDim() << ansiItalic();
                cout << (_json5 ? "_id" : "\"_id\"");
                cout << ansiReset() << ansiDim() << ": \"" << docID << "\"";
                if (revID) {
                    n++;
                    cout << ",\n" << subIndent << ansiItalic();
                    cout << (_json5 ? "_rev" : "\"_rev\"");
                    cout << ansiReset() << ansiDim() << ": \"" << revID << "\"";
                }
            }
            vector<slice> keys;
            for (Dict::iterator i(dict); i; ++i) {
                slice key = i.keyString();
                if (!onlyKeys || onlyKeys->find(alloc_slice(key)) != onlyKeys->end())
                    keys.push_back(key);
            }
            sort(keys.begin(), keys.end());
            for (slice key : keys) {
                if (n++ > 0)
                    cout << ',' << ansiReset();
                cout << '\n' << subIndent << ansiItalic();
                if (_json5 && canBeUnquotedJSON5Key(key))
                    cout << key;
                else
                    cout << '"' << key << '"';      //FIX: Escape quotes
                cout << ansiReset() << ": ";

                prettyPrint(dict.get(key), subIndent);
            }
            cout << '\n' << indent << "}";
            break;
        }
        case kFLArray: {
            string subIndent = indent + "  ";
            cout << "[\n";
            for (Array::iterator i(value.asArray()); i; ++i) {
                cout << subIndent;
                prettyPrint(i.value(), subIndent);
                if (i.count() > 1)
                    cout << ',';
                cout << '\n';
            }
            cout << indent << "]";
            break;
        }
        case kFLData: {
            // toJSON would convert to base64, which isn't readable, so use escapes instead:
            static const char kHexDigits[17] = "0123456789abcdef";
            slice data = value.asData();
            auto end = (const uint8_t*)data.end();
            cout << "«";
            bool dim = false;
            for (auto c = (const uint8_t*)data.buf; c != end; ++c) {
                if (*c >= 32 && *c < 127) {
                    if (dim)
                        cout << ansiReset();
                    dim = false;
                    cout << (char)*c;
                } else {
                    if (!dim)
                        cout << ansiDim();
                    dim = true;
                    cout << '\\' << kHexDigits[*c/16] << kHexDigits[*c%16];
                }
            }
            if (dim)
                cout << ansiReset();
            cout << "»";
            break;
        }
        default: {
            alloc_slice json(value.toJSON());
            cout << json;
            break;
        }
    }
}


#pragma mark - AUTOCOMPLETE:


template <class Callback>
static void findDocIDsMatching(C4Database *db, string pattern, Callback callback) {
    int errPos;
    C4Error error;
    c4::ref<C4Query> query = c4query_new2(db, kC4N1QLQuery,
                                          "SELECT _id WHERE _id LIKE $ID ORDER BY _id"_sl,
                                          &errPos, &error);
    if (!query) {
        C4Warn("(Command completion: failed to instantiate C4Query");
        return;
    }

    Encoder enc;
    enc.beginDict();
    enc["ID"] = pattern;
    enc.endDict();

    c4::ref<C4QueryEnumerator> e = c4query_run(query, nullptr, enc.finish(), &error);
    if (e) {
        while (c4queryenum_next(e, &error)) {
            slice docID = Array::iterator(e->columns)[0].asString();
            callback(docID);
        }
    }

    if (error.code) {
        C4Warn("(Command completion: error running query)");
        return;
    }
}


void CBLiteCommand::addDocIDCompletions(ArgumentTokenizer &tokenizer,
                                        std::function<void(const string&)> addCompletion)
{
    // Skip to the last uncompleted argument:
    string commandLine;
    while (tokenizer.spaceAfterArgument()) {
        commandLine += tokenizer.argument() + " ";
        if (!tokenizer.next()) {
            return;
        }
    }
    string arg = tokenizer.argument();
    if (arg[0] == '-')
        return; // Don't autocomplete a flag!

    auto argLen = arg.size();
    string completion;
    findDocIDsMatching(_db, arg + "%", [&](slice docID) {
        // Find where docID and completion differ:
        auto end = std::min(completion.size(), docID.size);
        auto i = argLen;
        while (i < end && docID[i] == completion[i])
            ++i;
        //cerr << "\n(docID='" << docID << "', comp='" << completion << "', j=" << j << ")";//TEMP
        if (i == argLen) {
            // There is no common prefix, so emit the current completion:
            if (!completion.empty()) {
                //cerr << "\n    --> '" << completion << "'";//TEMP
                addCompletion(commandLine + completion);
            }
            completion = docID;
        } else {
            // Truncate the completion to the common prefix:
            completion.resize(i);
        }
    });
    if (!completion.empty())
        addCompletion(commandLine + completion);
}
