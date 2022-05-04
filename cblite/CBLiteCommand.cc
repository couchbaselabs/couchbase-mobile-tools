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
