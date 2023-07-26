//
// MkCollCommand.cc
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
#include <algorithm>


using namespace std;
using namespace litecore;


class MkCollCommand : public CBLiteCommand {
public:

    MkCollCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("mkcoll", true, "[COLLECTION_PATH]");
        cerr <<
        "  Creates a collection, optionally specifying a parent scope.\n"
        "    COLLECTION_PATH can be either a collection name in the default collection,\n"
        "    or of the form <scope_name>/<collection_name>"
        ;
    }


    void runSubcommand() override {
        openWriteableDatabaseFromNextArg();
        do {
            string input = nextArg("collection name");
            auto [scope, coll] = getCollectionPath(input);

            C4Error error;
            C4CollectionSpec spec {slice(coll), slice(scope)};

            if (!c4db_createCollection(_db, spec, &error))
                fail("Couldn't create collection "+input, error);
            cout << "Created collection '" << scope << "/" << coll << "'.\n";
        } while (hasArgs());
    }
};


CBLiteCommand* newMkCollCommand(CBLiteTool &parent) {
    return new MkCollCommand(parent);
}

