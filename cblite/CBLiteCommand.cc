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
    #include <atlbase.h>
    #include <Shlwapi.h>
    #pragma comment(lib, "shlwapi.lib")
    #undef min
    #undef max
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
    if (CBLiteTool::processFlag(flag, specs)) {
        return true;
#ifdef HAS_COLLECTIONS
    } else if (flag == "--collection") {
        setCollectionName(nextArg("collection name"));
        return true;
    } else if (flag == "--scope") {
        setScopeName(nextArg("scope name"));
        return true;
#endif
    } else {
        return false;
    }
}


string CBLiteCommand::tempDirectory() {
#ifdef _MSC_VER
    WCHAR pathBuffer[MAX_PATH + 1];
    GetTempPathW(MAX_PATH, pathBuffer);
    GetLongPathNameW(pathBuffer, pathBuffer, MAX_PATH);
    CW2AEX<256> convertedPath(pathBuffer, CP_UTF8);
    return convertedPath.m_psz;
#else // _MSC_VER
    const char *tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp/";
#endif // _MSC_VER
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

#ifdef HAS_COLLECTIONS
C4Collection* CBLiteCommand::collection() {
    if (!_db) return nullptr;
    if (_collectionName.empty())
        return c4db_getDefaultCollection(_db);

    if (_scopeName.empty())
        return c4db_getCollection(_db, {slice(_collectionName), kC4DefaultScopeID});
   

    return c4db_getCollection(_db, {slice(_collectionName), slice(_scopeName)});
}


void CBLiteCommand::setCollectionName(const std::string &name) {
    _collectionName = name;
    if (_parent)
        _parent->setCollectionName(name);
}

void CBLiteCommand::setScopeName(const std::string &name) {
    _scopeName = name;
    if (_parent)
        _parent->setScopeName(name);
}
#endif


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
#ifdef HAS_COLLECTIONS
    c4::ref<C4Document> doc = c4coll_getDoc(collection(), slice(docID), true, content, &error);
#else
    c4::ref<C4Document> doc = c4db_getDoc(_db, slice(docID), true, content, &error);
#endif
    if (!doc && (error.domain != LiteCoreDomain || error.code != kC4ErrorNotFound))
        errorOccurred(format("reading document \"%s\"", docID.c_str()), error);
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


pair<string, string> CBLiteCommand::getCollectionPath(const string& input) const {
    if(auto slash = input.find('/'); slash != string::npos) {
        return make_pair(input.substr(0, slash), input.substr(slash + 1));
    }

    return make_pair(string(kC4DefaultScopeID), input);
}


int64_t CBLiteCommand::enumerateDocs(EnumerateDocsOptions options, EnumerateDocsCallback callback) {
    if (!options.pattern.empty() && !isGlobPattern(options.pattern)) {
        // Optimization when pattern has no metacharacters -- just get the one doc:
        string docID = options.pattern;
        unquoteGlobPattern(docID);
        c4::ref<C4Document> doc = readDoc(docID, kDocGetAll);
        if (!doc)
            return 0;
        alloc_slice docIDSlice(docID);
        C4DocumentInfo info = {};
        info.flags = doc->flags;
        info.docID = docIDSlice;
        info.sequence = doc->sequence;
        info.bodySize = c4doc_getRevisionBody(doc).size;
        info.expiration = c4doc_getExpiration(_db, slice(docID), nullptr);
        callback(info, (options.flags & kC4IncludeBodies) ? doc.get() : nullptr);
        return 1;
    }

    C4Error error;
    C4EnumeratorOptions c4Options = {options.flags};
    c4::ref<C4DocEnumerator> e;
#ifdef HAS_COLLECTIONS
    if (options.collection == nullptr)
        options.collection  = collection();
    if (options.bySequence)
        e = c4coll_enumerateChanges(options.collection, 0, &c4Options, &error);
    else
        e = c4coll_enumerateAllDocs(options.collection, &c4Options, &error);
#else
    if (options.bySequence)
        e = c4db_enumerateChanges(_db, 0, &c4Options, &error);
    else
        e = c4db_enumerateAllDocs(_db, &c4Options, &error);
#endif
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

        c4::ref<C4Document> doc;
        if (options.flags & kC4IncludeBodies) {
            doc = c4enum_getDocument(e, &error);
            if (!doc)
                fail("getting document " + string(slice(info.docID)), error);
        }

        callback(info, doc);
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
                                ostream &out,
                                const string &indent,
                                slice docID,
                                slice revID,
                                const set<alloc_slice> *onlyKeys)
{
    string reset, dim, italic;
    if (&out == &std::cout) {
        reset = ansiReset();
        dim = ansiDim();
        italic = ansiItalic();
    }

    switch (value.type()) {
        case kFLDict: {
            auto dict = value.asDict();
            string subIndent = indent + "  ";
            int n = 0;
            out << "{";
            if (docID) {
                n++;
                out << '\n' << subIndent << dim << italic;
                out << (_json5 ? "_id" : "\"_id\"");
                out << reset << dim << ": \"" << docID << "\"";
                if (revID) {
                    n++;
                    out << ",\n" << subIndent << italic;
                    out << (_json5 ? "_rev" : "\"_rev\"");
                    out << reset << dim << ": \"" << revID << "\"";
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
                    out << ',' << reset;
                out << '\n' << subIndent << italic;
                if (_json5 && canBeUnquotedJSON5Key(key))
                    out << key;
                else
                    out << '"' << key << '"';      //FIX: Escape quotes
                out << reset << ": ";

                prettyPrint(dict.get(key), out, subIndent);
            }
            out << '\n' << indent << "}";
            break;
        }
        case kFLArray: {
            string subIndent = indent + "  ";
            out << "[\n";
            for (Array::iterator i(value.asArray()); i; ++i) {
                out << subIndent;
                prettyPrint(i.value(), out, subIndent);
                if (i.count() > 1)
                    out << ',';
                out << '\n';
            }
            out << indent << "]";
            break;
        }
        case kFLData: {
            // toJSON would convert to base64, which isn't readable, so use escapes instead:
            static const char kHexDigits[17] = "0123456789abcdef";
            slice data = value.asData();
            auto end = (const uint8_t*)data.end();
            out << "«";
            bool dimmed = false;
            for (auto c = (const uint8_t*)data.buf; c != end; ++c) {
                if (*c >= 32 && *c < 127) {
                    if (dimmed)
                        out << reset;
                    dimmed = false;
                    out << (char)*c;
                } else {
                    if (!dimmed)
                        out << dim;
                    dimmed = true;
                    out << '\\' << kHexDigits[*c/16] << kHexDigits[*c%16];
                }
            }
            if (dimmed)
                out << reset;
            out << "»";
            break;
        }
        default: {
            alloc_slice json(value.toJSON());
            out << json;
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
