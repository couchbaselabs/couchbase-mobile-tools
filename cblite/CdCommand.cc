//
// CdCommand.cc
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


class CdCommand : public CBLiteCommand {
public:
    CdCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("cd", true, "[COLLECTION_PATH]");
        cerr <<
        "  Makes the collection the default for subsequent commands.\n"
        "    If empty, returns to the default collection.\n"
        "    COLLECTION_PATH can be either a collection name in the default collection,\n"
        "    or of the form <scope_name>/<collection_name>"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
        });

        openDatabaseFromNextArg();

        string coll, scope;
        if (peekNextArg().empty()) {
            cout << "You are in the default collection.\n";
        } else {
            auto input = nextArg("collection name");
            tie(scope, coll) = getCollectionPath(input);

            C4CollectionSpec spec {slice(coll), slice(scope)};
            C4Error err;
            if(!c4db_getCollection(_db, spec, &err)) {
                fail("Failed to retrieve " + scope + "/" + coll + " (" + to_string(err.domain) + " / " + to_string(err.code) + ")");
            }
        }

        setScopeName(scope);
        setCollectionName(coll);
    }


    void addLineCompletions(ArgumentTokenizer &tokenizer,
                            function<void(const string&)> add) override
    {
        addDocIDCompletions(tokenizer, add);
    }

}; // end catCommand


CBLiteCommand* newCdCommand(CBLiteTool &parent) {
    return new CdCommand(parent);
}
