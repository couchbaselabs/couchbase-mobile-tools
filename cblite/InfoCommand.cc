//
// InfoCommand.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
#include "c4Private.h"

using namespace fleece;


class delimiter {
public:
    explicit delimiter(const char *sep NONNULL = ", ")
    :delimiter(nullptr, sep, nullptr)
    { }

    explicit delimiter(const char *begin, const char *sep NONNULL, const char *end)
    :_separator(sep)
    ,_end(end)
    {
        if (begin)
            cout << begin;
    }

    operator const char*() {
        return (_count++ == 0) ? "" : _separator;
    }

    ~delimiter() {
        if (_end)
            cout << _end;
    }

private:
    const char* const _separator;
    const char* const _end = nullptr;
    int               _count = 0;
};


class InfoCommand : public CBLiteCommand {
public:
    InfoCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("info", false);
        cerr <<
        "  Displays information about the database, like sizes and counts.\n"
        "    --verbose or -v : Gives more detail.\n"
        ;
        writeUsageCommand("info", false, "indexes");
        cerr <<
        "  Lists all indexes and the values they index.\n"
        ;
        writeUsageCommand("info", false, "index NAME");
        cerr <<
        "  Displays information about index named NAME.\n"
        ;
    }


    uint64_t countDocsWhere(const char *what) {
        string n1ql = "SELECT count(*) WHERE "s + what;
        C4Error error;
        c4::ref<C4Query> q = c4query_new2(_db, kC4N1QLQuery, slice(n1ql), nullptr, &error);
        if (!q)
            fail("querying database", error);
        c4::ref<C4QueryEnumerator> e = c4query_run(q, nullptr, nullslice, &error);
        if (!e)
            fail("querying database", error);
        c4queryenum_next(e, &error);
        return FLValue_AsUnsigned(FLArrayIterator_GetValueAt(&e->columns, 0));
    }


    void getTotalDocSizes(uint64_t &dataSize, uint64_t &metaSize, uint64_t &conflictCount) {
        dataSize = metaSize = conflictCount = 0;
        enumerateDocs(kC4IncludeNonConflicted | kC4Unsorted | kC4IncludeBodies,
                      [&](C4DocEnumerator *e) {
            C4Error error;
            c4::ref<C4Document> doc = c4enum_getDocument(e, &error);
            if (!doc)
                fail("reading documents", error);
            dataSize += doc->selectedRev.body.size;
            C4DocumentInfo info;
            c4enum_getDocumentInfo(e, &info);
            metaSize += info.bodySize - doc->selectedRev.body.size;
            if (doc->flags & kDocConflicted)
                ++conflictCount;
            return true;
        });
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--verbose", [&]{verboseFlag();}},
            {"-v",        [&]{verboseFlag();}},
        });
        openDatabaseFromNextArg();

        if (peekNextArg() == "index") {
            nextArg(nullptr);
            if (hasArgs())
                indexInfo(nextArg("index name"));
            else
                indexInfo();
            return;
        } else if (peekNextArg() == "indexes") {
            indexInfo();
            return;
        }

        endOfArgs();

        // Database path:
        alloc_slice pathSlice = c4db_getPath(_db);
        cout << "Database:    " << pathSlice << "\n";

        // Overall sizes:
        uint64_t dbSize, blobsSize, nBlobs;
        getDBSizes(dbSize, blobsSize, nBlobs);
        cout << "Size:        ";
        writeSize(dbSize + blobsSize);
        cout << " ";
        {
            delimiter comma("(", ", ", ")");

            if (verbose()) {
                cout << comma << "doc bodies: ";
                cout.flush();
                uint64_t dataSize, metaSize, conflictCount;
                getTotalDocSizes(dataSize, metaSize, conflictCount);
                writeSize(dataSize);
                cout << ", doc metadata: ";
                writeSize(metaSize);
                if (conflictCount > 0)
                    cout << " (" << conflictCount << " conflicts!)";
                if (blobsSize > 0)
                    cout << ", ";
            }
            if (blobsSize > 0 || verbose()) {
                cout << comma << "blobs: ";
                writeSize(blobsSize);
            }
            if (!verbose())
                cout << comma << "use -v for more detail";
        }
        cout << "\n";

        // Document counts:
        cout << "Documents:   ";
        cout.flush(); // the next results may take a few seconds to print
        {
            delimiter comma(", ");
            cout << comma << c4db_getDocumentCount(_db);

            auto nDeletedDocs = countDocsWhere("_deleted");
            if (nDeletedDocs > 0)
                cout << " live" << comma << nDeletedDocs << " deleted";

            C4Timestamp nextExpiration = c4db_nextDocExpiration(_db);
            if (nextExpiration > 0) {
                cout << comma << countDocsWhere("_expiration > 0") << " with expirations";
                auto when = std::max((long long)nextExpiration - c4_now(), 0ll);
                cout << " (next in " << when << " sec)";
            }

            cout  << comma << "last sequence #" << c4db_getLastSequence(_db) << "\n";
        }

        if (nBlobs > 0) {
            cout << "Blobs:       " << nBlobs << ", ";
            writeSize(blobsSize);
            cerr << "\n";
        }

        // Indexes:
        alloc_slice indexesFleece = c4db_getIndexesInfo(_db, nullptr);
        auto indexes = Value::fromData(indexesFleece).asArray();
        if (indexes.count() > 0) {
            cout << "Indexes:     ";
            int n = 0;
            for (Array::iterator i(indexes); i; ++i) {
                if (n++)
                    cout << ", ";
                auto info = i.value().asDict();
                cout << info["name"].asString();
                auto type = C4IndexType(info["type"].asInt());
                if (type == kC4FullTextIndex)
                    cout << " [FTS]";
                if (type == kC4ArrayIndex)
                    cout << " [A]";
                else if (type == kC4PredictiveIndex)
                    cout << " [P]";
            }
            cout << "\n";
        }

        // UUIDs:
        C4UUID publicUUID, privateUUID;
        if (c4db_getUUIDs(_db, &publicUUID, &privateUUID, nullptr)) {
            cout << "UUIDs:       public "
                 << slice(&publicUUID, sizeof(publicUUID)).hexString().c_str()
                 << ", private " << slice(&privateUUID, sizeof(privateUUID)).hexString().c_str()
                 << "\n";
        }

        // Shared keys:
        auto sk = c4db_getFLSharedKeys(_db);
        FLSharedKeys_Decode(sk, 0);
        fleece::Doc stateDoc(alloc_slice(FLSharedKeys_GetStateData(sk)));
        auto keyArray = stateDoc.asArray();
        cout << "Shared keys: " << keyArray.count();
        if (!keyArray.empty()) {
            if (verbose()) {
                cout << ": ";
                delimiter next("[\"", "\", \"", "\"]");
                for (Array::iterator i(keyArray); i; ++i)
                    cout << next << i->asString();
            } else {
                cout << "  (-v to list them)";
            }
        }
        cout << '\n';
    }

    void indexInfo(const string &name ="") {
        alloc_slice indexesFleece = c4db_getIndexesInfo(_db, nullptr);
        auto indexes = Value::fromData(indexesFleece).asArray();
        bool any = false;
        for (Array::iterator i(indexes); i; ++i) {
            auto info = i.value().asDict();
            auto indexName = info["name"].asString();
            if (name.empty() || slice(name) == indexName) {
                cout << indexName;
                auto type = C4IndexType(info["type"].asInt());
                if (type == kC4FullTextIndex)
                    cout << " [FTS]";
                if (type == kC4ArrayIndex)
                    cout << " [Array]";
                else if (type == kC4PredictiveIndex)
                    cout << " [Predictive]";
                auto expr = info["expr"].asString();
                cout << "\n\t" << expr << "\n";
                any = true;
            }
        }

        if (!any) {
            if (name.empty())
                cout << "No indexes.\n";
            else
                cout << "No index \"" << name << "\".\n";
        } else if (!name.empty()) {
            // Dump the index:
            C4Error error;
            alloc_slice rowData(c4db_getIndexRows(_db, slice(name), &error));
            if (!rowData)
                fail("getting index rows", error);
            Doc doc(rowData);
            for (Array::iterator i(doc.asArray()); i; ++i) {
                auto row = i.value().asArray();
                cout << "    ";
                int c = 0;
                for (Array::iterator j(row); j; ++j) {
                    if (c++ > 0)
                        cout << "\t";
                    alloc_slice str(j.value().toString());
                    if (str.size > 0)
                        cout << str;
                    else
                        cout << "\"\"";
                }
                cout << "\n";
            }
        }
    }
    
};


CBLiteCommand* newInfoCommand(CBLiteTool &parent) {
    return new InfoCommand(parent);
}
