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
#include "fleece/Expert.hh"

#include "c4Collection.hh"
#include "c4Database.hh"
#include <iomanip>

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
        "    --verbose or -v : Gives more detail. Twice, gives even more detail.\n";
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
        if (nBlobs > 0) {
            cout << " (including ";
            writeSize(blobsSize);
            cout << " for " << nBlobs << " blobs)";
        }
        cout << "\n";

        if (verbose()) {
            // Versioning:
            auto config = c4db_getConfig2(_db);
            cout << "Versioning:  ";
            if (config->flags & kC4DB_VersionVectors) {
                alloc_slice peerID = c4db_getSourceID(_db);
                cout << "version vectors (source ID: @" << peerID << ")\n";
            } else {
                cout << "revision trees\n";
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

        // Collections:
        {
            cout << "Collections: ";
            cout.flush();
            auto collections = allCollections();
            if (verbose()) {
                CollectionStats totalStats;
                cout << ansiUnderline() << "       Name      #Docs     #Del   #Blobs   #Confl  Body Size  Meta Size" << ansiReset() << endl;
                for (CollectionSpec& spec : collections) {
                    cout << setw(24) << nameOfCollection(spec) << " : ";
                    cout.flush();
                    auto stats = getCollectionStats(spec);
                    cout << stats << endl;
                    totalStats += stats;
                }
                cout << ansiBold() << setw(24) << "TOTALS" << " : " << totalStats << ansiReset() << endl;

                if (verbose() >= 2) {
                    cout << "Document size distribution:\n";
                    for (size_t i = 0; i < std::size(totalStats.sizeHistogram); ++i) {
                        if (auto count = totalStats.sizeHistogram[i]) {
                            auto [n, scale] = scaleForSize(1 << i);
                            if (string_view(scale) == " bytes")
                                cout << "\t" << setw(5) << unsigned(n);
                            else
                                cout << "\t" << setw(3) << unsigned(n) << scale;
                            cout << " :" << setw(9) << count << endl;
                        }
                    }
                }
            } else {
                uint64_t totalDocs = 0;
                delimiter comma(", ");
                for (CollectionSpec& spec : collections) {
                    auto docCount = _db->getCollection(C4CollectionSpec(spec))->getDocumentCount();
                    totalDocs += docCount;
                    cout << comma << nameOfCollection(spec);
                    if (collections.size() > 1)
                        cout << " (" << docCount << ")";
                }
                cout << " -- " << totalDocs << " docs total\n";
            }
        }

        if (verbose()) {
            // Indexes:
            bool any = false;
            forEachIndex([&](CollectionSpec const& coll, string_view indexName,
                             string_view typeName, Dict info) {
                cout << (any ? ", " : "Indexes:     ");
                cout << coll.displayName() << '.';
                cout << indexName;
                if (typeName != "Value")
                    cout << " [" << typeName[0] << "]";
                any = true;
            });
            if (any)
                cout << endl;
        } else {
            cout << it("... use -v for more detail");
        }
    }


    void indexInfo() {
        bool any = false;
        forEachIndex([&](CollectionSpec const& coll, string_view indexName,
                         string_view typeName, Dict info) {
            string name = coll.displayName() + "." + string(indexName);
            if (arg.empty() || arg == name) {
                cout << name;
                cout << " : " << typeName << " index on `" << info["expr"].asString() << "`\n";
                any = true;

                if (_verbose && !arg.empty() && typeName.empty()) {
                    // Dump the index:
                    alloc_slice rowData = _db->getCollection(C4CollectionSpec(coll))->getIndexRows(indexName);
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
        });

        if (!any) {
            if (arg.empty())
                cout << "No indexes.\n";
            else
                cout << "No index \"" << arg << "\".\n";
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
        c4::ref<C4QueryEnumerator> e = c4query_run(q, nullslice, &error);
        if (!e)
            fail("querying database", error);
        [[maybe_unused]] bool _ = c4queryenum_next(e, &error);
        return FLValue_AsUnsigned(FLArrayIterator_GetValueAt(&e->columns, 0));
    }


    struct CollectionStats {
        uint64_t count= 0, deletedCount = 0, withAttachmentCount = 0, conflictCount = 0;
        uint64_t dataSize = 0, metaSize = 0;
        array<uint64_t,24> sizeHistogram = {}; // indexed by log2(dataSize+metaSize)

        CollectionStats& operator+=(const CollectionStats& other) {
            count += other.count;
            dataSize += other.dataSize;
            metaSize += other.metaSize;
            deletedCount += other.deletedCount;
            conflictCount += other.conflictCount;
            withAttachmentCount += other.withAttachmentCount;
            for (size_t i = 0; i < std::size(sizeHistogram); ++i)
                sizeHistogram[i] += other.sizeHistogram[i];
            return *this;
        }

        friend ostream& operator<<(ostream& out, const CollectionStats& stats) {
            return out << setw(8) << stats.count << ' '
                       << setw(8) << stats.deletedCount << ' '
                       << setw(8) << stats.withAttachmentCount << ' '
                       << setw(8) << stats.conflictCount << ' '
                       << setw(10) << stats.dataSize << ' '
                       << setw(10) << stats.metaSize;
        }
    };


    CollectionStats getCollectionStats(C4CollectionSpec const& spec) {
        CollectionStats stats;
        EnumerateDocsOptions options;
        options.collection = _db->getCollection(spec);
        options.flags |= kC4Unsorted | kC4IncludeDeleted;
        enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
            stats.count++;
            if (info.flags & kDocDeleted)        ++stats.deletedCount;
            if (info.flags & kDocHasAttachments) ++stats.withAttachmentCount;
            if (info.flags & kDocConflicted)     ++stats.conflictCount;
            stats.dataSize += info.bodySize;
            stats.metaSize += info.metaSize;

            auto i = std::min(64 - countl_zero(info.bodySize), 23);
            stats.sizeHistogram[i]++;
        });
        return stats;
    }


    fleece::Doc sharedKeysDoc() {
        auto sk = c4db_getFLSharedKeys(_db);
        FLSharedKeys_Decode(sk, 0);
        return fleece::Doc(alloc_slice(FLSharedKeys_GetStateData(sk)));
    }
 

    using IndexCallback = function_ref<void(CollectionSpec const&,
                                            string_view indexName,
                                            string_view typeName,
                                            FLDict info)>;

    void forEachIndex(IndexCallback callback) {
        for (CollectionSpec& spec : allCollections()) {
            C4Collection* coll = _db->getCollection(C4CollectionSpec(spec));
            alloc_slice indexesFleece = coll->getIndexesInfo();
            Array indexes = ValueFromData(indexesFleece).asArray();
            for (Array::iterator i(indexes); i; ++i) {
                auto info = i.value().asDict();
                string_view indexName = info["name"].asString();
                auto typeName = indexTypeName(info);
                callback(spec, indexName, typeName, info);
            }
        }
    }


    /// Returns a name for the type of the index described by Dict `info`
    string_view indexTypeName(Dict info) {
        static constexpr const char* kIndexTypeName[] = {
            "Value", "FTS", "Array", "Predictive", "Vector" };
        auto type = info["type"].asInt();
        if (type >= kC4ValueIndex && type <= kC4VectorIndex)
            return kIndexTypeName[type];
        else
            return "unknown";
    }
};


CBLiteCommand* newInfoCommand(CBLiteTool &parent) {
    return new InfoCommand(parent);
}
