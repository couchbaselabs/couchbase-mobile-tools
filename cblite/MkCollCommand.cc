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

#ifdef HAS_COLLECTIONS

using namespace std;
using namespace litecore;


class MkCollCommand : public CBLiteCommand {
public:

    MkCollCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("mkcoll", true, "NAME [SCOPE]");
        cerr <<
        "  Creates a collection, optionally specifying a parent scope.\n"
        ;
    }


    void runSubcommand() override {
        openWriteableDatabaseFromNextArg();
        string name = nextArg("collection name");
        string scope = peekNextArg();

        C4Error error;
        C4CollectionSpec spec {slice(name), kC4DefaultScopeID};
        if(!scope.empty()) {
            spec.scope = slice(scope);
        }

        if (!c4db_createCollection(_db, spec, &error))
            fail("Couldn't create collection", error);
        cout << "Created collection '" << name << "'.\n";
    }
};


CBLiteCommand* newMkCollCommand(CBLiteTool &parent) {
    return new MkCollCommand(parent);
}

#endif // HAS_COLLECTIONS
