//
// RmIndexCommand.cc
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


#include "CBLiteCommand.hh"
#include "fleece/Fleece.hh"
#include "fleece/Expert.hh"

using namespace std;
using namespace litecore;
using namespace fleece;


class RmIndexCommand : public CBLiteCommand {
public:

    RmIndexCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("rmindex", true, "NAME");
        cerr <<
        "  Deletes an index.\n"
        ;
    }


    void runSubcommand() override {
        openWriteableDatabaseFromNextArg();
        string name = nextArg("index name");

        // Workaround for `c4coll_deleteIndex` not reporting any error when index doesn't exist:
        if (!indexExists(name)) {
            string message = "No index named '" + name + "'";
            if (collection() != c4db_getDefaultCollection(_db)) {
                auto spec = c4coll_getSpec(collection());
                message += " in collection " + string(spec.scope) + "." + string(spec.name);
            }
                
            fail(message);
        }

        C4Error error;
        bool ok;
        ok = c4coll_deleteIndex(collection(), slice(name), &error);
        if (!ok)
            fail("Couldn't delete index", error);

        cout << "Deleted index '" << name << "'.\n";
    }


    bool indexExists(slice name) {
        alloc_slice indexesFleece;
        indexesFleece = c4coll_getIndexesInfo(collection(), nullptr);
        auto indexes = ValueFromData(indexesFleece).asArray();
        for (Array::iterator i(indexes); i; ++i) {
            auto info = i.value().asDict();
            if (info["name"].asString() == name)
                return true;
        }
        return false;
    }
};


CBLiteCommand* newRmIndexCommand(CBLiteTool &parent) {
    return new RmIndexCommand(parent);
}
