//
// RemotePutCommand.cc
//
// Copyright © 2022 Couchbase. All rights reserved.
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
#include "fleece/FLExpert.h"
#include <algorithm>

using namespace std;
using namespace litecore;


class RemotePutCommand : public CBRemoteCommand {
public:
    enum PutMode {kPut, kUpdate, kCreate, kDelete};


    RemotePutCommand(CBRemoteTool &parent, PutMode mode)
    :CBRemoteCommand(parent)
    ,_putMode(mode)
    { }


    void usage() override {
        writeUsageCommand("put", true, "DOCID \"JSON\"");
        cerr <<
        "  Updates a document.\n"
        "    --create : Document must not exist\n"
        "    --delete : Deletes the document (and JSON is optional); same as `rm` subcommand\n"
        "    --update : Document must already exist\n"
        "    -- : End of arguments (use if DOCID starts with '-')\n"
        "    " << it("DOCID") << " : Document ID\n"
        "    " << it("JSON") << " : Document body as JSON (JSON5 syntax allowed.) Must be quoted.\n"
        ;

        writeUsageCommand("rm", false, "DOCID");
        cerr <<
        "  Deletes a document. (Same as `put --delete`)\n"
        "    " << it("DOCID") << " : Document ID\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--create", [&]{_putMode = kCreate;}},
            {"--update", [&]{_putMode = kUpdate;}},
            {"--delete", [&]{_putMode = kDelete;}},
        });
        openDatabaseFromNextArg();

        string docID = nextArg("document ID");
        string json5;
        if (_putMode != kDelete)
            json5 = restOfInput("document body as JSON");
        endOfArgs();

        // Fleece-encode the body:
        alloc_slice fleeceBody;
        if (_putMode != kDelete) {
            FLStringResult errMsg;
            alloc_slice json = FLJSON5_ToJSON(slice(json5), &errMsg, nullptr, nullptr);
            if (!json) {
                string message = string(alloc_slice(errMsg));
                FLSliceResult_Release(errMsg);
                fail("Invalid JSON: " + message);
            }
            fleeceBody = fleece::Doc::fromJSON(json).allocedData();
            if (!fleeceBody)
                fail("Invalid JSON");
        }

        bool conflict;
        bool existed = false;
        string newRevID;
        do {
            conflict = false;
            // Get the existing revID on the server:
            alloc_slice revID;
            if (_putMode != kCreate) {
                Result<C4ConnectedClient::DocResponse> r = _db->getDoc(docID,
                                                                       nullslice, nullslice,
                                                                       true).blockingResult();
                if (r.ok()) {
                    existed = true;
                    revID = r.value().revID;
                } else if (r.error() == C4Error{LiteCoreDomain, kC4ErrorNotFound}) {
                    if (_putMode != kPut)
                        fail("Document doesn't exist on the server");
                } else {
                    fail("Couldn't access document", r.error());
                }
            }

            // Now save/delete:
            Result<string> r = _db->putDoc(docID, nullslice, revID,
                                           (_putMode == kDelete ? kRevDeleted : 0),
                                           fleeceBody).blockingResult();
            if (r.ok()) {
                newRevID = r.value();
            } else if (r.error() == C4Error{LiteCoreDomain, kC4ErrorConflict}) {
                if (_putMode == kCreate)
                    fail("Document already exists on the server");
                conflict = true;
            } else {
                fail("Couldn't update document", r.error());
            }
        } while (conflict);

        if (newRevID.size() > 10)
            newRevID.resize(10);

        if (_putMode == kDelete)
            cout << "Deleted";
        else if (existed)
            cout << "Updated";
        else
            cout << "Created";
        cout << " `" << docID << "`";
        if (_putMode != kDelete)
            cout << ", new revision " << newRevID << endl;
    }

private:
    PutMode                 _putMode {kPut};
};


CBRemoteCommand* newPutCommand(CBRemoteTool &parent) {
    return new RemotePutCommand(parent, RemotePutCommand::kPut);
}

CBRemoteCommand* newRmCommand(CBRemoteTool &parent) {
    return new RemotePutCommand(parent, RemotePutCommand::kDelete);
}
