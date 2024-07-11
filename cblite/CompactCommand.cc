//
// CompactCommand.cc
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
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
#include "DocBranchIterator.hh"
#include <algorithm>

using namespace std;
using namespace litecore;


class CompactCommand : public CBLiteCommand {
public:
    CompactCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("compact", false);
        cerr <<
        "  Compacts the database file, removing internal free space and deleting obsolete blobs.\n"
        "    --prune N : Also prunes revision trees to maximum depth N\n"
        "    --purgeDeleted : Also purges _all_ deleted documents\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--prune",         [&]{_prune = nextIntArg("depth for --prune", 1);}},
            {"--purgeDeleted",  [&]{_purgeDeleted = true;}},
        });

        openWriteableDatabaseFromNextArg();
        endOfArgs();

        auto logSizes = [&] {
            uint64_t dbSizeBefore, blobsSizeBefore, nBlobsBefore;
            getDBSizes(dbSizeBefore, blobsSizeBefore, nBlobsBefore);
            cout << setw(10) << dbSizeBefore << "  "
                 << setw(10) << blobsSizeBefore << "  "
                 << setw(10) << nBlobsBefore << "\n";
        };

        cout << "        " << ansiUnderline() << "   DB Size  Blobs size  Blob Count" << ansiReset() << "\n";
        cout << "Before: ";
        logSizes();

        if (_prune > 0 || _purgeDeleted) {
            tidy(_prune, _purgeDeleted);
        }

        cout << "--Compacting database file ...";
        cout.flush();
        C4Error error;
        if (!c4db_maintenance(_db, kC4Compact, &error))
            fail("compacting the database", error);
        cout << " done.\n";

        cout << "After:  ";
        logSizes();
    }


    // Subroutine of compact() that prunes rev-trees and/or purges deleted docs.
    void tidy(unsigned pruneToDepth, bool purgeDeleted) {
        cout.flush();
        c4::Transaction t(_db);
        C4Error error;
        if (!t.begin(&error))
            fail("opening a transaction", error);

        unsigned totalPrunedRevs = 0, totalRemovedBodies = 0, totalPurgedDocs = 0;
        vector<c4::ref<C4Document>> prunedDocs;
        vector<alloc_slice> deletedDocIDs;

        cout << "--Scanning ...";
        EnumerateDocsOptions options;
        options.flags |= kC4IncludeDeleted | kC4Unsorted;
        if (pruneToDepth != 0)
            options.flags |= kC4IncludeBodies;
        auto n = enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
    //        if (++n % 1000 == 0) {
    //            cout << "[" << n << "] ";
    //            cout.flush();
    //        }
            if (purgeDeleted) {
                if (info.flags & kDocDeleted) {
                    ++totalPurgedDocs;
                    deletedDocIDs.emplace_back(info.docID);
                    return;
                }
            }
            if (pruneToDepth != 0) {
                auto [nPrunedRevs, nRemovedBodies] = pruneDoc(doc, pruneToDepth);
                if (nPrunedRevs > 0 || nRemovedBodies > 0)
                    prunedDocs.emplace_back(c4doc_retain(doc));
                totalPrunedRevs += nPrunedRevs;
                totalRemovedBodies += nRemovedBodies;
            }
        });
        cout << " " << n << " docs; done.\n";

        if (pruneToDepth != 0) {
            if (!prunedDocs.empty()) {
                cout << "--Pruning " << totalPrunedRevs << " older revisions, and "
                     << totalRemovedBodies << " resolved conflicts ...";
                cout.flush();
                for (auto &doc : prunedDocs) {
                    if (!c4doc_save(doc, pruneToDepth, &error)) {
                        cerr << "\n*** Error " << error.domain << "/" << error.code
                             << " pruning doc '" << string(slice(doc->docID)) << "'\n";
                    }
                }
                cout << " done.\n";
            } else {
                cout << "  No revisions to prune.\n";
            }
        }

        if (purgeDeleted) {
            if (totalPurgedDocs > 0) {
                cout << "--Purging " << totalPurgedDocs << " deleted docs... ";
                cout.flush();
                for (auto &docID : deletedDocIDs) {
                    bool ok = c4coll_purgeDoc(collection(), docID, &error);
                    if (!ok) {
                        cerr << "\n*** Error " << error.domain << "/" << error.code
                             << " purging doc '" << string(docID) << "'\n";
                    }
                }
                cout << " done.\n";
            } else {
                cerr << "  No deleted documents to purge.\n";
            }
        }

        if (totalPrunedRevs + totalRemovedBodies + totalPurgedDocs > 0) {
            cout << "--Committing changes ...";
            cout.flush();
            if (!t.commit(&error))
                fail("committing a transaction", error);
            cout << " done.\n";
        } else {
            t.abort(nullptr);
        }
    }


    // Prunes a single document's rev-tree, without saving.
    pair<unsigned, unsigned> pruneDoc(C4Document *doc, unsigned maxDepth) {
        unsigned nPrunedRevs = 0, nRemovedBodies = 0;
        alloc_slice currentRevID = doc->selectedRev.revID;
        for (DocBranchIterator i(doc); i; ++i) {
            // Look at each branch of the document:
            if (doc->selectedRev.flags & kRevClosed) {
                // Closed conflict branch:
                alloc_slice closedBranch = doc->selectedRev.revID;
                alloc_slice branchPoint;
                if (c4doc_selectCommonAncestorRevision(doc, doc->selectedRev.revID, currentRevID))
                    branchPoint = doc->selectedRev.revID;
                // First count the number of revs on the branch:
                [[maybe_unused]] bool _ = c4doc_selectRevision(doc, closedBranch, false, nullptr);
                do {
                    ++nPrunedRevs;
                    if (doc->selectedRev.flags & kRevKeepBody)
                        ++nRemovedBodies;
                } while (c4doc_selectParentRevision(doc) && doc->selectedRev.revID != branchPoint);
                // Then prune the entire branch:
                _ = c4doc_purgeRevision(doc, closedBranch, nullptr);
            } else {
                // Walk its ancestor chain, counting how many revs are deeper than maxDepth:
                unsigned branchDepth = 1, keepBodyDepth = 0;
                while (c4doc_selectParentRevision(doc)) {
                    ++branchDepth;
                    if (doc->selectedRev.flags & kRevKeepBody) // ...but preserve revs leading to a body
                        keepBodyDepth = branchDepth;
                }
                if (branchDepth > maxDepth)
                    nPrunedRevs += branchDepth - max(keepBodyDepth, maxDepth);
            }
        }
        return {nPrunedRevs, nRemovedBodies};
    }

private:
    int                     _prune {0};
    bool                    _purgeDeleted {false};

};


CBLiteCommand* newCompactCommand(CBLiteTool &parent) {
    return new CompactCommand(parent);
}
