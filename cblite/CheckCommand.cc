//
// CheckCommand.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
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


class CheckCommand : public CBLiteCommand {
public:

    CheckCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("check", false);
        cerr <<
        "  Performs database integrity checks.\n"
        ;
    }


    void runSubcommand() override {
        openDatabaseFromNextArg();
        endOfArgs();

        cout << "Checking database integrity ... ";
        cout.flush();

        C4Error error;
        if (c4db_maintenance(_db, kC4IntegrityCheck, &error))
            cout << "everything looks OK!\n";
        else {
            cout << "\n";
            fail("Integrity check failed", error);
        }
    }

};


CBLiteCommand* newCheckCommand(CBLiteTool &parent) {
    return new CheckCommand(parent);
}
