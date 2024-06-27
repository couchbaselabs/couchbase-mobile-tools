//
// PutCommand.cc
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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
#include "fleece/FLExpert.h"
#include <algorithm>

using namespace std;
using namespace litecore;


class PutCommand : public CBLiteCommand {
public:
    enum PutMode {kPut, kUpdate, kCreate, kDelete, kPurge};


    PutCommand(CBLiteTool &parent, PutMode mode)
    :CBLiteCommand(parent)
    ,_putMode(mode)
    { }


    void usage() override {
        switch (_putMode) {
            default:
                writeUsageCommand("put", true, "DOCID \"JSON\"");
                cerr <<
                "  Updates a document.\n"
                "    --create : Document must not exist\n"
                "    --delete : Deletes the document (and JSON is optional); same as `rm` subcommand\n"
                "    --purge  : Purges the document (deletes without leaving a tombstone)\n"
                "    --update : Document must already exist\n"
                "    -- : End of arguments (use if DOCID starts with '-')\n"
                "    " << it("DOCID") << " : Document ID\n"
                "    " << it("JSON") << " : Document body as JSON (JSON5 syntax allowed.)\n"
                ;
                break;
            case kDelete:
                writeUsageCommand("rm", true, "DOCID");
                cerr <<
                "  Deletes a document. (Same as `put --delete`)\n"
                "    --purge  : Purges the document (doesn't leave a tombstone)\n"
                "    " << it("DOCID") << " : Document ID\n"
                ;
                break;
        }
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--create", [&]{_putMode = kCreate;}},
            {"--update", [&]{_putMode = kUpdate;}},
            {"--delete", [&]{_putMode = kDelete;}},
            {"--purge",  [&]{_putMode = kPurge;}},
        });
        openWriteableDatabaseFromNextArg();

        string docID = nextArg("document ID");
        string json5;
        if (_putMode < kDelete)
            json5 = restOfInput("document body as JSON");
        endOfArgs();

        C4Error error;
        c4::Transaction t(_db);
        if (!t.begin(&error))
            fail("Couldn't open database transaction");

        c4::ref<C4Document> doc;
        bool existed = false;
        if (_putMode == kPurge) {
            if (!c4coll_purgeDoc(collection(), slice(docID), &error))
                fail("Couldn't purge document", error);
        } else {
            doc = c4coll_getDoc(collection(), slice(docID), false, kDocGetAll, &error);
            if (!doc)
                fail("Couldn't read document", error);
            existed = (doc->flags & kDocExists) != 0
            && (doc->selectedRev.flags & kRevDeleted) == 0;
            if (!existed && (_putMode == kUpdate || _putMode == kDelete)) {
                if (doc->flags & kDocExists)
                    fail("Document is already deleted");
                else
                    fail("Document doesn't exist");
            }
            if (existed && _putMode == kCreate)
                fail("Document already exists");

            alloc_slice body;
            if (_putMode != kDelete) {
                FLStringResult errMsg;
                alloc_slice json = FLJSON5_ToJSON(slice(json5), &errMsg, nullptr, nullptr);
                if (!json) {
                    string message = string(alloc_slice(errMsg));
                    FLSliceResult_Release(errMsg);
                    fail("Invalid JSON: " + message);
                }
                body = c4db_encodeJSON(_db, json, &error);
                if (!body)
                    fail("Couldn't encode body", error);
            }

            doc = c4doc_update(doc, body, (_putMode == kDelete ? kRevDeleted : 0), &error);
            if (!doc)
                fail("Couldn't save document", error);
        }
        if (!t.commit(&error))
            fail("Couldn't commit database transaction", error);

        const char *verb;
        if (_putMode == kPurge) {
            cout << "Purged `" << docID << "`\n";
        } else {
            if (_putMode == kDelete)
                verb = "Deleted";
            else if (existed)
                verb = "Updated";
            else
                verb = "Created";
            string revID = slice(doc->selectedRev.revID).asString();
            if (revID.size() > 10)
                revID.resize(10);
            cout << verb << " `" << docID << "`, new revision " << revID
                 << " (sequence " << doc->sequence << ")\n";
        }
    }

private:
    PutMode                 _putMode {kPut};
};


CBLiteCommand* newPutCommand(CBLiteTool &parent) {
    return new PutCommand(parent, PutCommand::kPut);
}

CBLiteCommand* newRmCommand(CBLiteTool &parent) {
    return new PutCommand(parent, PutCommand::kDelete);
}
