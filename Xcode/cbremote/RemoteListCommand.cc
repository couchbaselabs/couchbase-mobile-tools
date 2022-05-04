//
// RemoteListCommand.cc
//
// Copyright (c) 2022 Couchbase, Inc All rights reserved.
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

#include "CBRemoteCommand.hh"
#include "ListCommand.hh"
#include "fleece/Fleece.hh"
#include <algorithm>
#include <set>

using namespace std;
using namespace fleece;
using namespace litecore;


class RemoteListCommand : public CBRemoteCommand {
public:
    RemoteListCommand(CBRemoteTool &parent)
    :CBRemoteCommand(parent)
    { }


    static constexpr int kListColumnWidth = 24;


    void usage() override {
        writeUsageCommand("ls", true, "[PATTERN]");
        cerr <<
        "  Lists all doc IDs, or those matching a glob-style pattern\n"
        "    -l : Long format (one doc per line, with metadata)\n"
        "    --offset N : Skip first N docs\n"
        "    --limit N : Stop after N docs\n"
        "    --desc : Descending order\n"
//        "    --seq : Order by sequence, not docID\n"
//        "    --del : Include deleted documents\n"
//        "    --conf : Show only conflicted documents\n"
        "    --body : Display document bodies\n"
        "    --pretty : Pretty-print document bodies (implies --body)\n"
        "    --json5 : JSON5 syntax, i.e. unquoted dict keys (implies --body)\n"
        "    " << it("PATTERN") << " : pattern for matching docIDs, with shell-style wildcards '*', '?'\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--offset", [&]{offsetFlag();}},
            {"--limit",  [&]{limitFlag();}},
            {"-l",       [&]{_longListing = true;}},
            {"--body",   [&]{bodyFlag();}},
            {"--pretty", [&]{prettyFlag();}},
            {"--raw",    [&]{rawFlag();}},
            {"--json5",  [&]{json5Flag();}},
            {"--desc",   [&]{descFlag();}},
            {"--seq",    [&]{_listBySeq = true;}},
            {"--del",    [&]{delFlag();}},
            {"--conf",   [&]{confFlag();}},
        });
        openDatabaseFromNextArg();
        string docIDPattern;
        if (hasArgs())
            docIDPattern = nextArg("docID pattern");
        endOfArgs();

        listDocs(docIDPattern);
    }


    void listDocs(string docIDPattern) {
        if (_offset > 0)
            cout << "(Skipping first " << _offset << " docs)\n";

        int xpos = 0;
        unsigned lineWidth = terminalWidth();
        bool firstDoc = true;
        int64_t nDocs = enumerateDocs(docIDPattern, [&](const C4DocumentInfo &info) {
            int idWidth = (int)terminalStringWidth(info.docID);
            if (_enumFlags & kC4IncludeBodies) {
                // 'cat' form:
                if (!firstDoc)
                    cout << "\n";

                firstDoc = false;
                catDoc(info.docID, true);

            } else if (_longListing) {
                // Long form:
                if (nDocs == 1) {
                    cout << ansi("4") << "Document ID             Rev ID     Flags   Seq     Size"
                         << ansiReset() << "\n";
                } else {
                    cout << "\n";
                }
                slice revID(info.revID.buf, min(info.revID.size, (size_t)10));
                cout << info.docID << spaces(kListColumnWidth - idWidth);
                cout << revID << spaces(10 - (int)revID.size);
                cout << ((info.flags & kDocDeleted)        ? 'd' : '-');
                cout << ((info.flags & kDocConflicted)     ? 'c' : '-');
                cout << ((info.flags & kDocHasAttachments) ? 'a' : '-');
                cout << ' ';
                cout << setw(7) << info.sequence << " ";
                cout << setw(7) << setprecision(1) << (info.bodySize / 1024.0) << 'K';

            } else {
                // Short form:
                int nSpaces = xpos ? (kListColumnWidth - (xpos % kListColumnWidth)) : 0;
                int newXpos = xpos + nSpaces + idWidth;
                if (newXpos < lineWidth) {
                    if (xpos > 0)
                        cout << spaces(nSpaces);
                    xpos = newXpos;
                } else {
                    cout << "\n";
                    xpos = idWidth;
                }
                cout << info.docID;
            }
        });

        if (nDocs == 0) {
            if (docIDPattern.empty())
                cout << "(No documents";
            else if (isGlobPattern(docIDPattern))
                cout << "(No documents with IDs matching \"" << docIDPattern << "\"";
            else
                cout << "(Document \"" << docIDPattern << "\" not found";
            if (!_collectionName.empty())
                cout << " in collection \"" << _collectionName << "\"";
            cout << ")";
        } else if (nDocs > _limit && _limit > 0) {
            cout << "\n(Stopping after " << _limit << " docs)";
        }
        cout << "\n";
    }


    int64_t enumerateDocs(slice docIDPattern, function<void(const C4DocumentInfo&)> callback) {
        vector<string> docIDs;
        C4Error error = {};
        mutex mut;
        condition_variable cond;
        unique_lock lock(mut);
        _db->getAllDocIDs(nullslice, docIDPattern, [&](const vector<slice>& ids,
                                                       const C4Error *err) {
            unique_lock lock(mut);
            if (!ids.empty()) {
                for (slice id : ids)
                    docIDs.emplace_back(id);
            } else {
                if (err)
                    error = *err;
                cond.notify_one();
            }
        });

        cond.wait(lock);
        if (error)
            fail("error from server", error);

        sort(docIDs.begin(), docIDs.end());
        if (_offset > 0)
            docIDs.erase(docIDs.begin(), docIDs.begin() + min(size_t(_offset), docIDs.size()));
        if (_limit >= 0 && docIDs.size() > _limit)
            docIDs.resize(_limit);

        C4DocumentInfo info {};
        for (const string &id : docIDs) {
            slice idSlice(id);
            info.docID = (C4HeapString&)idSlice; // ick
            callback(info);
        }

        return docIDs.size();
    }


    void catDoc(slice docID, bool includeID) {
        catDoc(readDoc(docID), includeID);
    }


    C4ConnectedClient::DocResponse readDoc(slice docID) {
        Result<C4ConnectedClient::DocResponse> r = _db->getDoc(docID,
                                                               nullslice, nullslice,
                                                               true).blockingResult();
        if (r.isError())
            fail("Couldn't get document", r.error());
        return r.value();
    }


    void catDoc(const C4ConnectedClient::DocResponse &doc, bool includeID) {
        Doc body(doc.body);
        if (!body)
            fail("Unexpectedly couldn't parse document body!");
        slice docID, revID;
        if (includeID || _showRevID)
            docID = slice(doc.docID);
        if (_showRevID)
            revID = (slice)doc.revID;
        if (_prettyPrint)
            prettyPrint(body, cout, "", docID, revID, (_keys.empty() ? nullptr : &_keys));
        else
            rawPrint(body, docID, revID);
    }


    bool                    _showRevID {false};
    bool                    _longListing {false};
    bool                    _listBySeq {false};
};


CBRemoteCommand* newListCommand(CBRemoteTool &parent) {
    return new RemoteListCommand(parent);
}
