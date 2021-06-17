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

#ifdef HAS_COLLECTIONS

using namespace std;
using namespace fleece;


class CdCommand : public CBLiteCommand {
public:
    CdCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("cd", true, "[COLLECTION]");
        cerr <<
        "  Makes the collection the default for subsequent commands.\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
        });

        openDatabaseFromNextArg();

        string name;
        if (peekNextArg().empty()) {
            cout << "You are in the default collection.\n";
        } else {
            name = nextArg("collection name");
            if (!c4db_getCollection(_db, slice(name)))
                fail("There is no collection \"" + name + "\".");
            cout << "You are in collection \"" << name << "\".\n";
        }
        setCollectionName(name);
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

#endif // HAS_COLLECTIONS
