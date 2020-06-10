//
// CatCommand.cc
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
#include "ListCommand.hh"
#include <algorithm>


class CatCommand : public ListCommand {
public:
    CatCommand(CBLiteTool &parent)
    :ListCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("cat", true, "DOCID [DOCID...]");
        cerr <<
        "  Displays the bodies of documents in JSON form.\n"
        "    --key KEY : Display only a single key/value (may be used multiple times)\n"
        "    --rev : Show the revision ID(s)\n"
        "    --raw : Raw JSON (not pretty-printed)\n"
        "    --json5 : JSON5 syntax (no quotes around dict keys)\n"
        "    -- : End of arguments (use if DOCID starts with '-')\n"
        "    " << it("DOCID") << " : Document ID, or pattern if it includes '*' or '?'\n"
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
                _enumFlags |= kC4IncludeBodies; // force displaying doc bodies
                listDocs(docID);
            } else {
                unquoteGlobPattern(docID); // remove any protective backslashes
                c4::ref<C4Document> doc = readDoc(docID);
                if (doc) {
                    catDoc(doc, includeIDs);
                    cout << '\n';
                }
            }
        }
    }

}; // end catCommand


CBLiteCommand* newCatCommand(CBLiteTool &parent) {
    return new CatCommand(parent);
}
