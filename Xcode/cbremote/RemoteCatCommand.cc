//
// CatCommand.cc
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


class RemoteCatCommand : public CBRemoteCommand {
public:
    RemoteCatCommand(CBRemoteTool &parent)
    :CBRemoteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("cat", true, "DOCID [DOCID...]");
        writeUsageCommand("get", true, "DOCID [DOCID...]");
        cerr <<
        "  Displays the bodies of documents in JSON form.\n"
        "    --key KEY : Display only a single key/value (may be used multiple times)\n"
        "    --rev : Show the revision ID\n"
        "    --raw : Raw JSON (not pretty-printed)\n"
        "    --json5 : JSON5 syntax (no quotes around dict keys)\n"
        "    -- : End of arguments (use if DOCID starts with '-')\n"
        "    " << it("DOCID") << " : Document ID\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--pretty", [&]{_prettyPrint = true;}},
            {"--raw",    [&]{_prettyPrint = false; _enumFlags |= kC4IncludeBodies;}},
            {"--json5",  [&]{_json5 = true; _enumFlags |= kC4IncludeBodies;}},
            {"--key",    [&]{keyFlag();}},
            {"--rev",    [&]{_showRevID = true;}},
        });

        openDatabaseFromNextArg();

        bool includeIDs = true;
        while (hasArgs()) {
            string docID = nextArg("document ID");
            if (isGlobPattern(docID)) {
                fail("Wildcards are not (yet?) supported");
            } else {
                unquoteGlobPattern(docID); // remove any protective backslashes
                C4ConnectedClient::DocResponse doc = readDoc(docID);
                catDoc(doc, includeIDs);
                cout << '\n';
            }
        }
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
};


CBRemoteCommand* newCatCommand(CBRemoteTool &parent) {
    return new RemoteCatCommand(parent);
}
