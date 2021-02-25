//
// RevsCommand.cc
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
#include "DocBranchIterator.hh"
using namespace std;
using namespace fleece;
using namespace litecore;


class RevsCommand : public CBLiteCommand {
public:
    using RevTree = map<alloc_slice,set<alloc_slice>>; // Maps revID to set of child revIDs
    using RemoteMap = vector<alloc_slice>;


    RevsCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("revs", false, "DOCID");
        cerr <<
        "  Shows a document's revision history\n"
        "    --remotes : Shows which revisions are known current on remote databases\n"
        "    -- : End of arguments (use if DOCID starts with '-')\n"
        "  Revision flags are denoted by dashes or the letters:\n"
        "    [D]eleted  [X]Closed  [C]onflict  [A]ttachments  [K]eep body  [L]eaf\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--remotes",   [&]{_showRemotes = true;}},
        });
        openDatabaseFromNextArg();
        string docID = nextArg("document ID");
        endOfArgs();

        auto doc = readDoc(docID);
        if (!doc)
            return;

        cout << "Document \"" << ansiBold() << doc->docID << ansiReset() << "\"";
        if (doc->flags & kDocDeleted)
            cout << ", Deleted";
        if (doc->flags & kDocConflicted)
            cout << ", Conflicted";
        if (doc->flags & kDocHasAttachments)
            cout << ", Has Attachments";
        cout << "\n";

        // Collect remote status:
        RemoteMap remotes;
        if (_showRemotes) {
            for (C4RemoteID remoteID = 1; true; ++remoteID) {
                alloc_slice revID(c4doc_getRemoteAncestor(doc, remoteID));
                if (!revID)
                    break;
                remotes.emplace_back(revID);
            }
        }

        // Collect revision tree info:
        RevTree tree;
        alloc_slice root; // use empty slice as root of tree
        int maxDepth = 0;
        int maxRevIDLen = 0;

        for (DocBranchIterator i(doc); i; ++i) {
            int depth = 1;
            alloc_slice childID = doc->selectedRev.revID;
            maxRevIDLen = max(maxRevIDLen, (int)childID.size);
            while (c4doc_selectParentRevision(doc)) {
                alloc_slice parentID(doc->selectedRev.revID);
                tree[parentID].insert(childID);
                childID = parentID;
                maxRevIDLen = max(maxRevIDLen, (int)childID.size);
                ++depth;
            }
            tree[root].insert(childID);
            maxDepth = max(maxDepth, depth);
        }

        int metaColumn = 2 * maxDepth + maxRevIDLen + 4;
        writeRevisionChildren(doc, tree, remotes, root, metaColumn, 1);

        for (C4RemoteID i = 1; i <= remotes.size(); ++i) {
            alloc_slice addr(c4db_getRemoteDBAddress(_db, i));
            if (!addr)
                break;
            cout << "[REMOTE#" << i << "] = " << addr << "\n";
        }
    }


    void writeRevisionTree(C4Document *doc,
                                       RevTree &tree,
                                       RemoteMap &remotes,
                                       alloc_slice root,
                                       int metaColumn,
                                       int indent)
    {
        C4Error error;
        if (!c4doc_selectRevision(doc, root, true, &error))
            fail("accessing revision", error);
        auto &rev = doc->selectedRev;
        cout << string(indent, ' ');
        cout << "* ";
        if ((rev.flags & kRevLeaf) && !(rev.flags & kRevClosed))
            cout << ansiBold();
        cout << rev.revID << ansiReset();

        int pad = max(2, metaColumn - int(indent + 2 + rev.revID.size));
        cout << string(pad, ' ');

        if (rev.flags & kRevClosed)
            cout << 'X';
        else
            cout << ((rev.flags & kRevDeleted)    ? 'D' : '-');
        cout << ((rev.flags & kRevIsConflict)     ? 'C' : '-');
        cout << ((rev.flags & kRevHasAttachments) ? 'A' : '-');
        cout << ((rev.flags & kRevKeepBody)       ? 'K' : '-');
        cout << ((rev.flags & kRevLeaf)           ? 'L' : '-');

        cout << " #" << rev.sequence;
        if (c4doc_getRevisionBody(doc).buf) {
            cout << ", ";
            writeSize(c4doc_getRevisionBody(doc).size);
        }

        if (root == slice(doc->revID))
            cout << ansiBold() << "  [CURRENT]" << ansiReset();

        C4RemoteID i = 1;
        for (alloc_slice &remote : remotes) {
            if (remote == root)
                cout << "  [REMOTE#" << i << "]";
            ++i;
        }

        cout << "\n";
        writeRevisionChildren(doc, tree, remotes, root, metaColumn, indent+2);
    }

    void writeRevisionChildren(C4Document *doc,
                                           RevTree &tree,
                                           RemoteMap &remotes,
                                           alloc_slice root,
                                           int metaColumn,
                                           int indent)
    {
        auto &children = tree[root];
        for (auto i = children.rbegin(); i != children.rend(); ++i) {
            writeRevisionTree(doc, tree, remotes, *i, metaColumn, indent);
        }
    }

private:
    bool                    _showRemotes {false};
};


CBLiteCommand* newRevsCommand(CBLiteTool &parent) {
    return new RevsCommand(parent);
}
