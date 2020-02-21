//
// cbliteTool+compact.cc
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

#include "cbliteTool.hh"
#include "c4Transaction.hh"
#include "DocBranchIterator.hh"
#include <algorithm>


const Tool::FlagSpec CBLiteTool::kCompactFlags[] = {
    {"--prune", (FlagHandler)&CBLiteTool::pruneFlag},
    {"--purgeDeleted", (FlagHandler)&CBLiteTool::purgeDeletedFlag},
    {nullptr, nullptr}
};

void CBLiteTool::compactUsage() {
    writeUsageCommand("compact", false);
    cerr <<
    "  Compacts the database file, removing internal free space and deleting obsolete blobs.\n"
    "    --prune N : Also prunes revision trees to maximum depth N\n"
    "    --purgeDeleted : Also purges _all_ deleted documents\n"
    ;
}


void CBLiteTool::compact() {
    // Read params:
    processFlags(kCompactFlags);
    if (_showHelp) {
        compactUsage();
        return;
    }
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
    if (!c4db_compact(_db, &error))
        fail("compacting the database", error);

    cout << " done.\n";

    // After vacuuming, the SQLite database file does not actually shrink until it's closed:
    alloc_slice databasePath = c4db_getPath(_db);
    auto config = *c4db_getConfig(_db);
    if (!c4db_close(_db, &error))
        fail("closing the database", error);
    _db = c4db_open(databasePath, &config, &error);
    if (!_db)
        fail("reopening the database", error);

    cout << "After:  ";
    logSizes();
}


void CBLiteTool::tidy(unsigned pruneToDepth, bool purgeDeleted) {
    cout.flush();
    c4::Transaction t(_db);
    C4Error error;
    if (!t.begin(&error))
        fail("opening a transaction", error);

    unsigned totalPrunedRevs = 0, totalRemovedBodies = 0, totalPurgedDocs = 0;
    uint64_t n = 0;
    vector<c4::ref<C4Document>> prunedDocs;
    vector<alloc_slice> deletedDocIDs;

    cout << "--Scanning ...";
    C4EnumeratorFlags enumFlags = kC4IncludeNonConflicted | kC4IncludeDeleted | kC4Unsorted;
    if (pruneToDepth != 0)
        enumFlags |= kC4IncludeBodies;
    enumerateDocs(enumFlags, [&](C4DocEnumerator *e) {
        ++n;
//        if (++n % 1000 == 0) {
//            cout << "[" << n << "] ";
//            cout.flush();
//        }
        if (purgeDeleted) {
            C4DocumentInfo info;
            c4enum_getDocumentInfo(e, &info);
            if (info.flags & kDocDeleted) {
                ++totalPurgedDocs;
                deletedDocIDs.emplace_back(info.docID);
                return true;
            }
        }
        if (pruneToDepth != 0) {
            c4::ref<C4Document> doc = c4enum_getDocument(e, &error);
            if (!doc)
                fail("reading a document", error);
            auto [nPrunedRevs, nRemovedBodies] = pruneDoc(doc, pruneToDepth);
            if (nPrunedRevs > 0 || nRemovedBodies > 0)
                prunedDocs.emplace_back(c4doc_retain(doc));
            totalPrunedRevs += nPrunedRevs;
            totalRemovedBodies += nRemovedBodies;
        }
        return true;
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
                if (!c4db_purgeDoc(_db, docID, &error)) {
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


pair<unsigned, unsigned> CBLiteTool::pruneDoc(C4Document *doc, unsigned maxDepth) {
    unsigned nPrunedRevs = 0, nRemovedBodies = 0;
    for (DocBranchIterator i(doc); i; ++i) {
        // Look at each branch of the document:
        bool branchClosed = (doc->selectedRev.flags & kRevClosed) != 0;
        // Walk its ancestor chain, counting how many revs are deeper than maxDepth:
        unsigned branchDepth = 1;
        while (c4doc_selectParentRevision(doc)) {
            bool keepBody = (doc->selectedRev.flags & kRevKeepBody) != 0;
            if (branchClosed && keepBody) {
                // Remove bodies of resolved conflicting revisions, which CBL 2.0-2.7
                // mistakenly preserved when resolving conflicts:
                c4doc_removeRevisionBody(doc);
                ++nRemovedBodies;
                keepBody = false;
            }
            if (++branchDepth > maxDepth && !keepBody)
                ++nPrunedRevs;
        }
    }
    return {nPrunedRevs, nRemovedBodies};
}
