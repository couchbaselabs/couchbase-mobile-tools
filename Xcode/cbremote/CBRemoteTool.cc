//
// CBRemoteTool.cc
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

#include "CBRemoteTool.hh"
#include "CBRemoteCommand.hh"
#include "c4Private.h"              // For C4RegisterBuiltInWebSocket()
#include "StringUtil.hh"            // for digittoint(), on non-BSD-like systems
#include <thread>

using namespace litecore;
using namespace std;
using namespace fleece;

int main(int argc, const char * argv[]) {
    CBRemoteTool tool;
    return tool.main(argc, argv);
}


CBRemoteTool::~CBRemoteTool() {
    if (_shouldCloseDB && _db) {
        _db->stop();
    }
}


void CBRemoteTool::usage() {
    cerr <<
    ansiBold() << "cbremote: Remote CLI to Couchbase Mobile databases\n\n" << ansiReset() <<
    bold("Usage:\n") <<
    "  cbremote " << it("[FLAGS] SUBCOMMAND DB_URL ...") << "\n"
    "  cbremote " << it("[FLAGS] DB_URL ...") << "       (interactive mode*)\n"
    "\n"
    "  " << it("DB_URL") << " is a database URL of the form wss://host:port/dbname\n"
    "\n" <<
    bold("Global Flags") << " (before the subcommand name):\n"
    "  --color     : Use bold/italic (sometimes color), if terminal supports it\n"
    "\n" <<
    bold("Subcommands:\n") <<
    "    cat, get       : display document body(ies) as JSON\n"
//    "    edit           : update or create a document as JSON in a text editor\n"
    "    help           : print more help for a subcommand\n"
    "    ls             : list document IDs\n"
    "    open           : open DB and start interactive mode*\n"
    "    put            : create or modify a document\n"
//    "    query, select  : run a N1QL or JSON query\n"
    "    rm             : delete documents\n"
    "\n"
    "    Most subcommands take their own flags or parameters, following the name.\n"
    "    For details, run `cbremote help " << it("SUBCOMMAND") << "`.\n"
    "\n" <<
    bold("Interactive Mode:\n") <<
    "  * The interactive 'shell' displays a `(cbremote)` prompt and lets you enter a subcommand.\n"
    "    Since the database is already open, you don't have to give its path again, nor any of\n"
    "    the global flags, simply the subcommand and any flags or parameters it takes.\n"
    "    For example: `cat doc123`, `help put`.\n"
    "    Exit the shell by entering `quit` or pressing Ctrl-D (on Unix) or Ctrl-Z (on Windows).\n"
    ;
}


void CBRemoteTool::displayVersion() {
    alloc_slice version = c4_getVersion();
    cout << "Couchbase Lite Core " << version << "\n";
    exit(0);
}


int CBRemoteTool::run() {
    // Initial pre-subcommand flags:
    processFlags({
        {"--version",   [&]{displayVersion();}},
        {"-v",          [&]{displayVersion();}},
    });

//    c4log_setCallbackLevel(kC4LogWarning);
    if (!hasArgs()) {
        cerr << ansiBold()
             << "cbremote: Remote CLI to Couchbase Mobile databases\n" << ansiReset()
             << "ERROR: Missing subcommand or database URL.\n"
             << "For a list of subcommands, run " << ansiBold() << "cbremote help" << ansiReset() << ".\n"
             << "To start the interactive mode, run "
             << ansiBold() << "cbremote " << ansiItalic() << "wss://host:port/dbname" << ansiReset() << '\n';
        fail();
    }

    string cmd = nextArg("subcommand or database path");
    if (isDatabaseURL(cmd)) {
        endOfArgs();
        CBRemoteCommand::runInteractive(*this, cmd);
    } else if (cmd == "help") {
        // Run "help" subcommand:
        helpCommand();
    } else {
        // Run subcommand:
        auto subcommandInstance = subcommand(cmd);
        if (subcommandInstance) {
            subcommandInstance->runSubcommand();
        } else {
            failMisuse(format("Unknown subcommand '%s'", cmd.c_str()));
        }
    }
    return 0;
}


#pragma mark - OPENING DATABASE:


bool CBRemoteTool::isDatabaseURL(const string &str) {
    C4Address addr;
    slice dbName;
    return C4Address::fromURL(slice(str), &addr, &dbName);
}


void CBRemoteTool::openDatabase(const string &urlStr, bool interactive) {
    C4RegisterBuiltInWebSocket();

    C4ConnectedClientParameters params = {};
    params.url = slice(urlStr);
    params.callbackContext = this;
    _db = C4ConnectedClient::newClient(params);

    //FIXME: Workaround until C4ConnectedClient has a delegate API
    C4ConnectedClientStatus status;
    do {
        this_thread::sleep_for(100ms);
        status = _db->getStatus().blockingResult().value();
    } while (status.level == kC4Connecting);
    if (status.level != kC4Idle && status.level != kC4Busy) {
        fail(format("Couldn't connect to database <%s>", urlStr.c_str()), status.error);
    }
    
    _dbURL = urlStr;
    _shouldCloseDB = true;
}


#pragma mark - COMMANDS:


void CBRemoteTool::helpCommand() {
    if (hasArgs()) {
        string currentCommand = nextArg("subcommand");
        auto subcommandInstance = this->subcommand(currentCommand);
        if (subcommandInstance)
            subcommandInstance->usage();
        else
            cerr << format("Unknown subcommand '%s'\n", currentCommand.c_str());
    } else {
        usage();
    }
}


using ToolFactory = CBRemoteCommand* (*)(CBRemoteTool&);

static constexpr struct {const char* name; ToolFactory factory;} kSubcommands[] = {
    {"cat",     newCatCommand},
    {"get",     newCatCommand},
    {"ls",      newListCommand},
    {"open",    newOpenCommand},
    {"put",     newPutCommand},
    {"rm",      newRmCommand},
};


unique_ptr<CBRemoteCommand> CBRemoteTool::subcommand(const string &name) {
    for (const auto &entry : kSubcommands) {
        if (name == entry.name) {
            unique_ptr<CBRemoteCommand> command( entry.factory(*this) );
            command->setName(name);
            return command;
        }
    }
    return nullptr;
}


void CBRemoteTool::addLineCompletions(ArgumentTokenizer &tokenizer,
                                    std::function<void(const string&)> addCompletion)
{
    string arg = tokenizer.argument();
    if (!tokenizer.spaceAfterArgument()) {
        // Incomplete subcommand name:
        for (const auto &entry : kSubcommands) {
            if (strncmp(arg.c_str(), entry.name, arg.size()) == 0)
                addCompletion(string(entry.name) + " ");
        }
    } else {
        // Instantiate subcommand and ask it to complete:
        if (auto cmd = subcommand(arg)) {
            tokenizer.next();
            cmd->addLineCompletions(tokenizer, [&](const string &completion) {
                // Prepend the previous arg to the subcommand's completion string:
                addCompletion(arg + " " + completion);
            });
        }
    }
}
