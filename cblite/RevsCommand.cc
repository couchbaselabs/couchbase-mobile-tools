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
        "    --raw : Shows revIDs as-is without translation\n"
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
            {"--raw",       [&]{_prettyPrint = false;}},
        });
        openDatabaseFromNextArg();
        string docID = nextArg("document ID");
        endOfArgs();

        _doc = readDoc(docID, kDocGetAll);
        if (!_doc)
            fail("Document not found");

        cout << "Document \"" << ansiBold() << _doc->docID << ansiReset() << "\"";
        if (_doc->flags & kDocDeleted)
            cout << ", Deleted";
        if (_doc->flags & kDocConflicted)
            cout << ", Conflicted";
        if (_doc->flags & kDocHasAttachments)
            cout << ", Has Attachments";
        cout << "\n";

        // Collect remote status:
        if (_showRemotes) {
            for (C4RemoteID remoteID = 1; true; ++remoteID) {
                alloc_slice revID(c4doc_getRemoteAncestor(_doc, remoteID));
                if (!revID)
                    break;
                _remotes.emplace_back(revID);
            }
        }

        if (usingVersionVectors())
            writeVersions();
        else
            writeRevTree();

        if (_showRemotes) {
            for (C4RemoteID i = 1; i <= _remotes.size(); ++i) {
                alloc_slice addr(c4db_getRemoteDBAddress(_db, i));
                if (addr)
                    cout << ansiDim() << "[REMOTE#" << i << "] = " << addr << ansiReset() << "\n";
            }
        }
    }


    void writeRevTree() {
        alloc_slice root; // use empty slice as root of tree
        int maxDepth = 0;
        int maxRevIDLen = 0;

        // Build a tree structure in _tree, and collect some maxima:
        for (DocBranchIterator i(_doc); i; ++i) {
            int depth = 1;
            alloc_slice childID = _doc->selectedRev.revID;
            maxRevIDLen = max(maxRevIDLen, (int)childID.size);
            while (c4doc_selectParentRevision(_doc)) {
                alloc_slice parentID(_doc->selectedRev.revID);
                _tree[parentID].insert(childID);
                childID = parentID;
                maxRevIDLen = max(maxRevIDLen, (int)childID.size);
                ++depth;
            }
            _tree[root].insert(childID);
            maxDepth = max(maxDepth, depth);
        }
        _metaColumn = 2 * maxDepth + maxRevIDLen + 4;

        // Now write the tree recursively:
        writeRevisionChildren(root, 1);
    }


    void writeRevisionChildren(alloc_slice revID, int indent) {
        auto &children = _tree[revID];
        for (auto i = children.rbegin(); i != children.rend(); ++i) {
            writeRevision(*i, indent);
        }
    }


    void writeRevision(alloc_slice revID, int indent) {
        C4Error error;
        if (!c4doc_selectRevision(_doc, revID, true, &error))
            fail("accessing revision", error);
        writeSelectedRevision(revID, indent);
        writeRevisionChildren(revID, indent+2);
    }


    void writeSelectedRevision(slice displayedRevID, int indent) {
        auto &rev = _doc->selectedRev;
        cout << string(indent, ' ');
        cout << "* ";
        if ((rev.flags & kRevLeaf) && !(rev.flags & kRevClosed))
            cout << ansiBold();
        cout << formatRevID(displayedRevID, _prettyPrint) << ansiReset();

        int pad = max(2, _metaColumn - int(indent + 2 + rev.revID.size));
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

        if (slice body = c4doc_getRevisionBody(_doc)) {
            cout << ", ";
            writeSize(body.size);
        }

        if (rev.revID == slice(_doc->revID))
            cout << ansiBold() << "  [CURRENT]" << ansiReset();

        C4RemoteID i = 1;
        for (alloc_slice &remote : _remotes) {
            if (remote == rev.revID)
                cout << "  [REMOTE#" << i << "]";
            ++i;
        }

        cout << "\n";
    }


    void writeVersions() {
        _metaColumn = 0;
        c4doc_selectCurrentRevision(_doc);
        do {
            _metaColumn = std::max(_metaColumn, 2 + int(_doc->selectedRev.revID.size));
        } while (c4doc_selectNextRevision(_doc));

        set<alloc_slice> seenRevIDs;
        c4doc_selectCurrentRevision(_doc);
        do {
            if (seenRevIDs.insert(alloc_slice(_doc->selectedRev.revID)).second) {
                alloc_slice vector = c4doc_getRevisionHistory(_doc, 0, nullptr, 0);
                writeSelectedRevision(vector, 1);
            }
        } while (c4doc_selectNextRevision(_doc));
    }


    void addLineCompletions(ArgumentTokenizer &tokenizer,
                            std::function<void(const std::string&)> add) override
    {
        addDocIDCompletions(tokenizer, add);
    }


private:
    bool _showRemotes {false};
    c4::ref<C4Document> _doc;
    RevTree _tree;
    int  _metaColumn;
    RemoteMap _remotes;
};


CBLiteCommand* newRevsCommand(CBLiteTool &parent) {
    return new RevsCommand(parent);
}
