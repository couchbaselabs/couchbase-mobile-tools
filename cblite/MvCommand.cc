//
// MvCommand.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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
#include "ListCommand.hh"
#include "fleece/Fleece.hh"
#include <algorithm>
#include <set>

using namespace std;
using namespace fleece;


class MvCommand : public CBLiteCommand {
public:
    MvCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("mv", true, "[DOC_PATH] [COLLECTION_PATH]");
        cerr <<
        "  Moves a document to another collection.\n"
        "    COLLECTION_PATH can be either a collection name in the default collection,\n"
        "    or of the form <scope_name>/<collection_name>"
        "    DOC_PATH can be either a doc ID (for documents in the default collection),\n"
        "    <collection_name>/<doc_id> for documents in a non-default collection in default scope,\n"
        "    or <scope_name>/<collection_name>/<doc_id> for a fully qualified document"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
        });

        openDatabaseFromNextArg();

        string srcDocID, dstDocID;
        C4Collection *srcColl, *dstColl;
        tie(srcDocID, srcColl) = splitPath(nextArg("doc ID"), true);
        tie(dstDocID, dstColl) = splitPath(nextArg("collection"), false);

        // TODO: Accept > 2 arguments, like real `mv`

        C4Error error;
        if (isGlobPattern(srcDocID)) {
            if (!dstDocID.empty())
                fail("Final argument must be a collection name");
            EnumerateDocsOptions options = {};
            options.collection = srcColl;
            options.pattern = srcDocID;
            c4::Transaction t(_db);
            if (!t.begin(&error))
                fail("begin transaction");

            auto n = enumerateDocs(options, [&](const C4DocumentInfo &i, C4Document*) {
                if (!c4coll_moveDoc(srcColl, i.docID,
                                    dstColl, i.docID, &error))
                    fail("Couldn't move document \"" + string(i.docID) + "\"", error);
            });

            if (n == 0) {
                cerr << "(No docIDs matched)\n";
            } else {
                if (!t.commit(&error))
                    fail("commit transaction");

                auto spec = c4coll_getSpec(dstColl);
                string dstName(spec.name);
                string dstScope(spec.scope);
                cout << "Moved " << n << " documents to \"" << dstScope << "." << dstName << "\".\n";
            }

        } else {
            // Move a single document:
            unquoteGlobPattern(srcDocID);
            if (dstDocID.empty())
                dstDocID = srcDocID;

            if (!c4coll_moveDoc(srcColl, slice(srcDocID),
                                dstColl, slice(dstDocID), &error))
                fail("Couldn't move document", error);
        }
    }


    pair<string,C4Collection*> splitPath(const string &arg, bool expectDocID) {
        string docID, collName, scope;
        if(!expectDocID) {
            tie(scope, collName) = getCollectionPath(arg);
        } else {
             if (auto slash = arg.rfind('/'); slash != string::npos) {
                 docID = arg.substr(slash + 1);
                 auto remainder = arg.substr(0, slash);
                 tie(scope, collName) = getCollectionPath(remainder);
             } else {
                docID = arg;
             }
        }

        C4Collection *coll;
        if (expectDocID && collName.empty()) {
            coll = collection();
        } else {
            coll = c4db_getCollection(_db, {slice(collName), slice(scope)});
            if (!coll)
                fail("There is no collection \"" + collName + "\".");
        }

        return {docID, coll};
    }


}; // end catCommand


CBLiteCommand* newMvCommand(CBLiteTool &parent) {
    return new MvCommand(parent);
}
