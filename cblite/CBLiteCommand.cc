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
#include "c4Database.hh"
#include <chrono>

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
    } else if (flag == "--collection") {
        setCollectionName(nextArg("collection name"));
        return true;
    } else if (flag == "--scope") {
        setScopeName(nextArg("scope name"));
        return true;
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

C4Collection* CBLiteCommand::collection() {
    if (!_db) return nullptr;
    if (_collectionName.empty())
        return c4db_getDefaultCollection(_db, nullptr);

    if (_scopeName.empty())
        return _db->getCollection({slice(_collectionName), slice(kC4DefaultScopeID)});
   

    return _db->getCollection({slice(_collectionName), slice(_scopeName)});
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


string CBLiteCommand::nameOfCollection() {
    if (_scopeName.empty() || slice(_scopeName) == kC4DefaultScopeID) {
        if (_collectionName.empty())
            return string(kC4DefaultCollectionName);
        else
            return _collectionName;
    } else {
        return _scopeName + "." + _collectionName;
    }
}


string CBLiteCommand::nameOfCollection(C4CollectionSpec spec) {
    string name(spec.name);
    if (spec.scope != kC4DefaultScopeID)
        name = string(spec.scope) + "." + name;
    return name;
}


bool CBLiteCommand::usingVersionVectors() const {
    return (c4db_getConfig2(_db)->flags & kC4DB_VersionVectors) != 0;
}



std::string CBLiteCommand::formatRevID(fleece::slice revid, bool pretty) {
    if (!usingVersionVectors() || !pretty) {
        // Default, for rev-tree revID or anything non-pretty:
        return string(revid);
    }

    // Version, pretty. This may be a version vector, so we break it up at delimiters:
    string result;
    slice vector = revid;
    do {
        // Get the next version from the vector (or just the single version):
        auto nextDelim = std::min( vector.findByteOrEnd(','), vector.findByteOrEnd(';') );

        C4RevIDInfo info;
        if (!c4rev_getInfo(slice(vector.buf, nextDelim), &info)) {
            // Invalid revID; instead of throwing, just return the raw string:
            return string(revid);
        }

        auto curSize = result.size();
        result.resize(curSize + 100, 0); // make room to append formatted data below
        auto dst = result.data() + curSize;
        size_t len;
        slice source;
        if (info.version.legacyGen > 0) {
            len = snprintf(dst, 100, "%u", info.version.legacyGen);
            source = "legacy";
        } else {
            len = strftime(dst, 100, "%F+%T", localtime(&info.version.clockTime));
            source = info.version.sourceString;
        }
        result.resize(curSize + len);

        result += "@";
        result += string_view(source);

        // Move the start of `vector` past the delimiter and any whitespace:
        vector.setStart(nextDelim);
        if (vector.size > 0) {
            vector.moveStart(1); // skip delim
            while (vector.hasPrefix(" "))
                vector.moveStart(1);
            result += *(char*)nextDelim; // output the delimeter
            result += ' ';
        }
    } while (vector.size > 0);
    return result;
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


uint64_t CBLiteCommand::countDocsWhere(C4CollectionSpec coll, const char *what) {
    string n1ql = format("SELECT count(*) FROM `%s` WHERE %s",
                         nameOfCollection(coll).c_str(), what);
    C4Error error;
    c4::ref<C4Query> q = c4query_new2(_db, kC4N1QLQuery, slice(n1ql), nullptr, &error);
    if (!q)
        fail("querying database", error);
    c4::ref<C4QueryEnumerator> e = c4query_run(q, nullslice, &error);
    if (!e)
        fail("querying database", error);
    c4queryenum_next(e, &error);
    return FLValue_AsUnsigned(FLArrayIterator_GetValueAt(&e->columns, 0));
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

    c4::ref<C4QueryEnumerator> e = c4query_run(query, enc.finish(), &error);
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
