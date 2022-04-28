//
// RemoteOpenCommand.cc
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

using namespace fleece;
using namespace std;
using namespace litecore;


/// The subcommand that runs the interactive mode.
class OpenCommand : public CBRemoteCommand {
public:

    OpenCommand(CBRemoteTool &parent)
    :CBRemoteCommand(parent)
    { }

    void usage() override {
        writeUsageCommand("open", false);
        cerr <<
        "  Opens the database and enters interactive mode, where you can run multiple commands\n"
        "  without having to retype the database path. Exit with `quit` or Ctrl-D.\n"
        ;
    }

    virtual bool interactive() const override       {return _interactive;}


    void runSubcommand() override {
        openDatabaseFromNextArg();
        endOfArgs();

        cout << "Opened remote database " << _dbURL << '\n';

        _interactive = true;
        while (true) {
            try {
                if (!readLine(bold("(cblite) ").c_str()))
                    return;
                string cmd = nextArg("subcommand");
                if (cmd == "quit") {
                    return;
                } else if (cmd == "help") {
                    helpCommand();
                } else if (cmd == "open" || cmd == "openremote") {
                    fail("A database is already open");
                } else {
                    auto subcommandInstance = subcommand(cmd);
                    if (subcommandInstance) {
                        subcommandInstance->runSubcommand();
                    } else {
                        cerr << format("Unknown subcommand '%s'; type 'help' for a list of commands.\n",
                                       cmd.c_str());
                    }
                }
            } catch (const exit_error &) {
                // subcommand exited; continue
            } catch (const fail_error &) {
                // subcommand failed (error message was already printed); continue
            }
        }
    }


    void helpCommand() override {
        if (hasArgs()) {
            CBRemoteTool::helpCommand();
            return;
        }

        cout << bold("Subcommands:") << "\n" <<
        "    cat " << it("[FLAGS] DOCID [DOCID...]") << "\n"
        "    get " << it("[FLAGS] DOCID [DOCID...]") << "\n"
        "    ls " << it("[FLAGS] [PATTERN]") << "\n"
        "    put " << it("[FLAGS] DOCID JSON_BODY") << "\n"
        "    rm " << it("DOCID") << "\n"
        "For more details, enter `help` followed by a subcommand name.\n"
        ;
    }


private:
    bool _interactive = false;
};


CBRemoteCommand* newOpenCommand(CBRemoteTool &parent) {
    return new OpenCommand(parent);
}


void CBRemoteCommand::runInteractive(CBRemoteTool &parent) {
    OpenCommand cmd(parent);
    cmd.runSubcommand();
}


void CBRemoteCommand::runInteractive(CBRemoteTool &parent, const string &databaseURL) {
    OpenCommand cmd(parent);
    cmd.openDatabase(databaseURL, true);
    cmd.runSubcommand();
}
