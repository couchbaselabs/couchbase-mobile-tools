//
// OpenCommand.cc
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

using namespace fleece;
using namespace std;
using namespace litecore;


/// The subcommand that runs the interactive mode.
class OpenCommand : public CBLiteCommand {
public:

    OpenCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
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
        
        const char *mode = (_dbFlags & kC4DB_ReadOnly) ? "read-only" : "writeable";
        cout << "Opened " << mode << " database " << alloc_slice(c4db_getPath(_db)) << '\n';

        _interactive = true;
        while (true) {
            try {
                string prompt;
                if (_collectionName.empty() || slice(_collectionName) == kC4DefaultCollectionName)
                    prompt = bold("(cblite) ");
                else {
                    prompt = bold("(cblite ") + "/ " + nameOfCollection() + ") ";
                }

                if (!readLine(prompt.c_str()))
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
            CBLiteTool::helpCommand();
            return;
        }

        cout << bold("Subcommands:") << "\n" <<
        "    cat " << it("[FLAGS] DOCID [DOCID...]") << "\n"
        "    cd " << it("COLLECTION") << "\n"
        "    check\n"
        "    compact\n"
        "    cp " << it("[FLAGS] DESTINATION") << "\n"
#ifdef COUCHBASE_ENTERPRISE
        "    decrypt\n"
        "    encrypt " << it("[FLAGS]") << "\n"
#endif
        "    edit " << it("[FLAGS] DOCID") << "\n"
        "    export " << it("[FLAGS] JSONFILE") << "\n"
        "    enrich " << it("[FLAGS] PROPERTY [DESTINATION]") << "\n"
        "    get " << it("[FLAGS] DOCID [DOCID...]") << "\n"
        "    help " << it("[SUBCOMMAND]") << "\n"
        "    import " << it("[FLAGS] JSONFILE") << "\n"
        "    info " << it("[FLAGS] [indexes] [index NAME]") << "\n"
        "    logcat " << it("[FLAGS] LOG_PATH [...]") << "\n"
        "    ls " << it("[FLAGS] [PATTERN]") << "\n"
        "    lscoll\n"
        "    mkcoll " << it("COLLECTION") << "\n"
        "    mv " << it("DOCID COLLECTION") << "\n"
        "    mkindex " << it("[FLAGS] INDEX_NAME EXPRESSION") << "\n"
        "    pull " << it("[FLAGS] SOURCE") << "\n"
        "    push " << it("[FLAGS] DESTINATION") << "\n"
        "    put " << it("[FLAGS] DOCID JSON_BODY") << "\n"
        "    query " << it("[FLAGS] JSON_QUERY") << "\n"
        "    quit\n"
        "    reindex\n"
        "    revs " << it("DOCID") << "\n"
        "    rm " << it("DOCID") << "\n"
        "    rmindex " << it("INDEX_NAME") << "\n"
        "    select " << it("[FLAGS] N1QLQUERY") << "\n"
        "    serve " << it("[FLAGS]") << "\n"
        //  "    sql " << it("QUERY") << "\n"
        "For more details, enter `help` followed by a subcommand name.\n\n"
        "Online docs: " << ansiUnderline() <<
        "https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/Documentation.md"
        << ansiReset() << endl;
    }


private:
    bool _interactive = false;
};


CBLiteCommand* newOpenCommand(CBLiteTool &parent) {
    return new OpenCommand(parent);
}


void CBLiteCommand::runInteractive(CBLiteTool &parent) {
    OpenCommand cmd(parent);
    cmd.runSubcommand();
}


void CBLiteCommand::runInteractive(CBLiteTool &parent, const string &databasePath) {
    OpenCommand cmd(parent);
    cmd.openDatabase(databasePath, true);
    cmd.runSubcommand();
}
