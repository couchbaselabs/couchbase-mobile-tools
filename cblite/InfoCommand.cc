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

#ifdef HAS_COLLECTIONS
#include "c4Collection.hh"
#include "c4Database.hh"
#endif

using namespace fleece;
using namespace std;
using namespace litecore;


#if LITECORE_API_VERSION >= 300
    #include "Delimiter.hh"
#else
    class delimiter {
    public:
        delimiter(const char *str =",") :_str(str) {}

        int count() const               {return _count;}
        const char* string() const      {return _str;}

        int operator++()                {return ++_count;} // prefix ++
        int operator++(int)             {return _count++;} // postfix ++

        const char* next()              {return (_count++ == 0) ? "" : _str;}

    private:
        int _count = 0;
        const char* const _str;
    };


    static inline std::ostream& operator<< (std::ostream &out, delimiter &delim) {
        if (delim++ > 0)
            out << delim.string();
        return out;
    }
#endif


class delimiter_wrapper : public delimiter {
public:
    explicit delimiter_wrapper(const char *begin NONNULL, const char *sep NONNULL,
                               const char *end NONNULL, ostream &out = std::cout)
    :delimiter(sep), _out(out), _end(end)   {_out << begin;}
    ~delimiter_wrapper()                    {_out << _end;}

private:
    ostream&          _out;
    const char* const _end;
};


class InfoCommand : public CBLiteCommand {
public:
    string arg;


    InfoCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("info", false);
        cerr <<
        "  Displays information about the database, like sizes and counts.\n"
        "    --verbose or -v : Gives more detail.\n";
        writeUsageCommand("info", false, "indexes");
        cerr <<
        "  Lists all indexes and the values they index.\n";
        writeUsageCommand("info", false, "index NAME");
        cerr <<
        "  Displays information about index named NAME.\n";
        writeUsageCommand("info", false, "keys");
        cerr <<
        "  Lists all Fleece shared keys used by documents.\n";
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--verbose", [&]{verboseFlag();}},
            {"-v",        [&]{verboseFlag();}},
        });

        void (InfoCommand::*subSubCommand)() = &InfoCommand::generalInfo;

        if (matchArg("index")) {
            arg = nextArg("index name");
            if (isDatabasePath(arg))
                fail("Missing argument: expected index name after `info index`");
            subSubCommand = &InfoCommand::indexInfo;
        } else if (matchArg("indexes")) {
            subSubCommand = &InfoCommand::indexInfo;
        } else if (matchArg("keys")) {
            subSubCommand = &InfoCommand::sharedKeysInfo;
        }

        openDatabaseFromNextArg();
        endOfArgs();

        (this->*subSubCommand)();
    }


    void generalInfo() {
        openDatabaseFromNextArg();
        endOfArgs();

        // Database path:
        {
            alloc_slice pathStr(c4db_getPath(_db));
            FilePath path(string(pathStr), "");
            cout << "Database:    " << path.canonicalPath() << "\n";
        }

        // Overall sizes:
        uint64_t dbSize, blobsSize, nBlobs;
        getDBSizes(dbSize, blobsSize, nBlobs);
        cout << "Size:        ";
        writeSize(dbSize + blobsSize);
        cout << "  ";
        {
            delimiter_wrapper comma("(", ", ", ")");

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
            if (nBlobs > 0 || verbose()) {
                cout << comma << "blobs: ";
                writeSize(blobsSize);
            }
            if (!verbose())
                cout << comma << it("use -v for more detail");
        }
        cout << "\n";

#ifdef HAS_COLLECTIONS
        cout << "Collections: ";
        delimiter lines("             ");
        for (string &collectionName : _db->getCollectionNames()) {
            // Document counts:
            cout << lines << '"' << bold(collectionName.c_str()) << "\": ";
            cout.flush(); // the next results may take a few seconds to print
            {
                auto coll = _db->getCollection(collectionName);
                delimiter comma(", ");
                cout << comma << coll->getDocumentCount() << " documents";

                if (coll == _db->getDefaultCollection()) {
                    //TODO: Do this for each collection once queries work
                    auto nDeletedDocs = countDocsWhere("_deleted");
                    if (nDeletedDocs > 0)
                        cout << " alive" << comma << nDeletedDocs << " deleted";
                }

                C4Timestamp nextExpiration = coll->nextDocExpiration();
                if (nextExpiration > 0) {
                    cout << comma << countDocsWhere("_expiration > 0") << " with expirations";
                    auto when = std::max((long long)nextExpiration - c4_now(), 0ll);
                    cout << ansiItalic() << " (next in " << when << " sec)" << ansiReset();
                }

                cout  << comma << "last sequence #" << coll->getLastSequence() << "\n";
            }
        }
#else
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
#endif

        if (nBlobs > 0) {
            cout << "Blobs:       " << nBlobs << "; ";
            writeSize(blobsSize);
            cerr << "\n";
        }

        if (verbose()) {
            // Versioning:
#if LITECORE_API_VERSION >= 300
            auto config = c4db_getConfig2(_db);
            cout << "Versioning:  ";
            if (config->flags & kC4DB_VersionVectors) {
                alloc_slice peerID = c4db_getPeerID(_db);
                cout << "version vectors (source ID: @" << peerID << ")\n";
            } else {
                cout << "revision trees\n";
            }
#endif

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
            cout << "Shared keys: " << sharedKeysDoc().asArray().count() << '\n';
        }
    }


    void indexInfo() {
        alloc_slice indexesFleece = c4db_getIndexesInfo(_db, nullptr);
        auto indexes = Value::fromData(indexesFleece).asArray();
        bool any = false;
        for (Array::iterator i(indexes); i; ++i) {
            auto info = i.value().asDict();
            auto indexName = info["name"].asString();
            if (arg.empty() || slice(arg) == indexName) {
                cout << indexName;
                auto type = C4IndexType(info["type"].asInt());
                if (type == kC4FullTextIndex)
                    cout << " [FTS]";
                if (type == kC4ArrayIndex)
                    cout << " [Array]";
                else if (type == kC4PredictiveIndex)
                    cout << " [Predictive]";
                auto expr = info["expr"].asString();
                cout << ":\n\t" << expr << "\n";
                any = true;
            }
        }

        if (!any) {
            if (arg.empty())
                cout << "No indexes.\n";
            else
                cout << "No index \"" << arg << "\".\n";
        } else if (!arg.empty()) {
            // Dump the index:
            C4Error error;
            alloc_slice rowData(c4db_getIndexRows(_db, slice(arg), &error));
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


    void sharedKeysInfo() {
        fleece::Doc keysDoc = sharedKeysDoc();
        auto keyArray = keysDoc.asArray();
        cout << keyArray.count() << " shared keys: ";
        delimiter_wrapper next("[\"", "\", \"", "\"]\n");
        for (Array::iterator i(keyArray); i; ++i)
            cout << next << i->asString();
    }


#pragma mark - UTILITIES:


    uint64_t countDocsWhere(const char *what) {
        string n1ql = "SELECT count(*) FROM _ WHERE "s + what;
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
        EnumerateDocsOptions options;
        options.flags |= kC4Unsorted;
#if LITECORE_API_VERSION < 300
        options.flags |= kC4IncludeBodies;
#endif
        enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
#if LITECORE_API_VERSION < 300
            auto revSize = doc->selectedRev.body.size;
            dataSize += revSize;
            metaSize += info.bodySize - revSize;
#else
            dataSize += info.bodySize;
            metaSize += info.metaSize;
#endif
            if (info.flags & kDocConflicted)
                ++conflictCount;
        });
    }


    fleece::Doc sharedKeysDoc() {
        auto sk = c4db_getFLSharedKeys(_db);
        FLSharedKeys_Decode(sk, 0);
        return fleece::Doc(alloc_slice(FLSharedKeys_GetStateData(sk)));
    }
    
};


CBLiteCommand* newInfoCommand(CBLiteTool &parent) {
    return new InfoCommand(parent);
}
