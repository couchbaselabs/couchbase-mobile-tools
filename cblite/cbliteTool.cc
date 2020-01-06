//
// cbliteTool.cc
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

#include "cbliteTool.hh"


int main(int argc, const char * argv[]) {
    CBLiteTool tool;
    return tool.main(argc, argv);
}


void CBLiteTool::usage() {
    cerr <<
    ansiBold() << "cblite: Couchbase Lite / LiteCore database multi-tool\n" << ansiReset() <<
    "Usage: cblite help " << it("[SUBCOMMAND]") << "\n"
    "       cblite cat " << it("[FLAGS] DBPATH DOCID [DOCID...]") << "\n"
    "       cblite cp " << it("[FLAGS] SOURCE DESTINATION") << "\n"
    "       cblite file " << it("[FLAGS] DBPATH") << "\n"
    "       cblite ls " << it("[FLAGS] DBPATH [PATTERN]") << "\n"
    "       cblite put " << it("DBPATH DOCID \"JSON\"") << "\n"
    "       cblite query " << it("[FLAGS] DBPATH JSONQUERY") << "\n"
    "       cblite revs " << it("DBPATH DOCID") << "\n"
    "       cblite rm " << it("DBPATH DOCID [\"JSON\"]") << "\n"
    "       cblite serve " << it("DBPATH") << "\n"
//  "       cblite sql " << it("DBPATH QUERY") << "\n"
    "       cblite [--create] " << it("DBPATH") << "   (interactive shell)\n"
    "             --create: Creates the database if it doesn't already exist.\n"
    "The shell accepts the same commands listed above, but without the 'cblite'\n"
    "and DBPATH parameters. For example, 'ls -l'.\n"
    "For information about subcommand parameters/flags, run `cblite help SUBCOMMAND`.\n"
    ;
}


void CBLiteTool::writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs) {
    cerr << ansiBold();
    if (!_interactive)
        cerr << "cblite ";
    cerr << cmd << ' ' << ansiItalic();
    if (hasFlags)
        cerr << "[FLAGS]" << ' ';
    if (!_interactive)
        cerr << "DBPATH ";
    cerr << otherArgs << ansiReset() << "\n";
}


void CBLiteTool::versionFlag() {
    alloc_slice version = c4_getVersion();
    cout << "Couchbase Lite Core " << version << "\n";
    exit(0);
}


int CBLiteTool::run() {
    c4log_setCallbackLevel(kC4LogWarning);
    clearFlags();
    if (!hasArgs()) {
        cerr << ansiBold()
             << "cblite: Couchbase Lite / LiteCore database multi-tool\n" << ansiReset() 
             << "Missing subcommand or database path.\n"
             << "For a list of subcommands, run " << ansiBold() << "cblite help" << ansiReset() << ".\n"
             << "To start the interactive mode, run "
             << ansiBold() << "cblite " << ansiItalic() << "DBPATH" << ansiReset() << '\n';
        fail();
    }

    string cmd = nextArg("subcommand or database path");
    if (isDatabasePath(cmd)) {
        endOfArgs();
        _interactive = true;
        openDatabase(cmd);
        runInteractively();
    } else {
        _currentCommand = cmd;
        if (!processFlag(cmd, kSubcommands)) {
            _currentCommand = "";
            if (cmd.find(FilePath::kSeparator) != string::npos || cmd.find('.') || cmd.size() > 10)
                fail(format("Not a valid database path (must end in %s) or subcommand name: %s",
                            kC4DatabaseFilenameExtension, cmd.c_str()));
            else
                failMisuse(format("Unknown subcommand '%s'", cmd.c_str()));
        }
    }
    return 0;
}

bool CBLiteTool::isDatabasePath(const string &path) {
    return hasSuffix(FilePath(path).fileOrDirName(), kC4DatabaseFilenameExtension);
}


void CBLiteTool::openDatabase(string path) {
#ifndef _MSC_VER
    if (hasPrefix(path, "~/")) {
        path.erase(path.begin(), path.begin()+1);
        path.insert(0, getenv("HOME"));
    }
#endif
    if (!isDatabasePath(path))
        fail("Database filename must have a '.cblite2' extension");
    C4DatabaseConfig config = {_dbFlags};
    C4Error err;
    _db = c4db_open(c4str(path), &config, &err);
    while (!_db && (err.code == kC4ErrorNotADatabaseFile && err.domain == LiteCoreDomain)) {
#ifdef COUCHBASE_ENTERPRISE
        // Database is encrypted
        if (!_interactive) {
            // Don't prompt for a password unless this is an interactive session
            fail("Database is encrypted (use interactive mode to open it)");
        }
        const char *prompt = "Database password:";
        if (config.encryptionKey.algorithm != kC4EncryptionNone)
            prompt = "Sorry, try again: ";
        string password = readPassword(prompt);
        if (password.empty())
            exit(1);
        if (c4key_setPassword(&config.encryptionKey, slice(password), kC4EncryptionAES256))
            _db = c4db_open(c4str(path), &config, &err);
        else
            cout << "Error: Couldn't derive key from password\n";
#else
        fail("Database is encrypted (Enterprise Edition is required to open encrypted databases)");
#endif
    }
    if (!_db)
        fail(format("Couldn't open database %s", path.c_str()), err);
}


void CBLiteTool::openDatabaseFromNextArg() {
    if (!_db)
        openDatabase(nextArg("database path"));
}


void CBLiteTool::openWriteableDatabaseFromNextArg() {
    if (_db) {
        if (_dbFlags & kC4DB_ReadOnly)
            fail("Database opened read-only; run `cblite --writeable` to allow writes");
    } else {
        _dbFlags &= ~kC4DB_ReadOnly;
        openDatabaseFromNextArg();
    }
}


#pragma mark - INTERACTIVE MODE:


// Flags that can come before the subcommand name:
const Tool::FlagSpec CBLiteTool::kPreCommandFlags[] = {
    {"--create",    (FlagHandler)&CBLiteTool::createDBFlag},
    {"--writeable", (FlagHandler)&CBLiteTool::writeableFlag},
    {"--encrypted", (FlagHandler)&CBLiteTool::encryptedFlag},
    {"--version",   (FlagHandler)&CBLiteTool::versionFlag},
    {nullptr, nullptr}
};


void CBLiteTool::shell() {
    // Read params:
    openDatabaseFromNextArg();
    endOfArgs();
    runInteractively();
}


void CBLiteTool::runInteractively() {
    _interactive = true;
    const char *mode = (_dbFlags & kC4DB_ReadOnly) ? "read-only" : "writeable";
    cout << "Opened " << mode << " database " << alloc_slice(c4db_getPath(_db)) << '\n';

    while(true) {
        try {
            if (!readLine("(cblite) "))
                return;
            string cmd = nextArg("subcommand");
            clearFlags();
            _currentCommand = cmd;
            if (!processFlag(cmd, kInteractiveSubcommands))
                cerr << format("Unknown subcommand '%s'; type 'help' for a list of commands.\n",
                               cmd.c_str());
        } catch (const fail_error &) {
            // subcommand failed (error message was already printed); continue
        }
    }
}


void CBLiteTool::helpCommand() {
    if (hasArgs()) {
        _showHelp = true; // forces command to show help and return
        _currentCommand = nextArg("subcommand");
        if (!processFlag(_currentCommand, kSubcommands))
            cerr << format("Unknown subcommand '%s'\n", _currentCommand.c_str());
    } else if (_interactive) {
        cout << bold("Subcommands:") << "\n" <<
        "    help " << it("[SUBCOMMAND]") << "\n"
        "    quit\n" <<
        "    cat " << it("[FLAGS] DOCID [DOCID...]") << "\n"
        "    cp " << it("[FLAGS] DESTINATION") << "\n"
#ifdef COUCHBASE_ENTERPRISE
        "    decrypt\n"
        "    encrypt " << it("[FLAGS]") << "\n"
#endif
        "    file " << it("[FLAGS]") << "\n"
        "    ls " << it("[FLAGS] [PATTERN]") << "\n"
        "    put " << it("DOCID \"JSON_BODY\"") << "\n"
        "    query " << it("[FLAGS] JSON_QUERY") << "\n"
        "    select " << it("N1QL_QUERY") << "\n"
        "    revs " << it("DOCID") << "\n"
        "    rm " << it("DOCID") << "\n"
        "    serve\n"
        "For more details, enter `help` followed by a subcommand name.\n"
        ;
    } else {
        usage();
    }
}


void CBLiteTool::quitCommand() {
    exit(0);
}


#pragma mark - FLAGS & SUBCOMMANDS:


const Tool::FlagSpec CBLiteTool::kSubcommands[] = {
    {"cat",     (FlagHandler)&CBLiteTool::catDocs},
    {"cp",      (FlagHandler)&CBLiteTool::copyDatabase},
    {"push",    (FlagHandler)&CBLiteTool::copyDatabase},
    {"export",  (FlagHandler)&CBLiteTool::copyDatabase},
    {"pull",    (FlagHandler)&CBLiteTool::copyDatabaseReversed},
    {"import",  (FlagHandler)&CBLiteTool::copyDatabaseReversed},
    {"file",    (FlagHandler)&CBLiteTool::fileInfo},
    {"help",    (FlagHandler)&CBLiteTool::helpCommand},
    {"ls",      (FlagHandler)&CBLiteTool::listDocsCommand},
    {"put",     (FlagHandler)&CBLiteTool::putDoc},
    {"query",   (FlagHandler)&CBLiteTool::queryDatabase},
    {"revs",    (FlagHandler)&CBLiteTool::revsInfo},
    {"rm",      (FlagHandler)&CBLiteTool::putDoc},
    {"select",  (FlagHandler)&CBLiteTool::queryDatabase},
    {"SELECT",  (FlagHandler)&CBLiteTool::queryDatabase},
    {"serve",   (FlagHandler)&CBLiteTool::serve},
    {"sql",     (FlagHandler)&CBLiteTool::sqlQuery},

    {"shell",   (FlagHandler)&CBLiteTool::shell},
    
#ifdef COUCHBASE_ENTERPRISE
    {"decrypt", (FlagHandler)&CBLiteTool::decrypt},
    {"encrypt", (FlagHandler)&CBLiteTool::encrypt},
#endif
    {nullptr, nullptr}
};

const Tool::FlagSpec CBLiteTool::kInteractiveSubcommands[] = {
    {"cat",     (FlagHandler)&CBLiteTool::catDocs},
    {"cp",      (FlagHandler)&CBLiteTool::copyDatabase},
    {"push",    (FlagHandler)&CBLiteTool::copyDatabase},
    {"export",  (FlagHandler)&CBLiteTool::copyDatabase},
    {"pull",    (FlagHandler)&CBLiteTool::copyDatabaseReversed},
    {"import",  (FlagHandler)&CBLiteTool::copyDatabaseReversed},
    {"file",    (FlagHandler)&CBLiteTool::fileInfo},
    {"help",    (FlagHandler)&CBLiteTool::helpCommand},
    {"ls",      (FlagHandler)&CBLiteTool::listDocsCommand},
    {"put",     (FlagHandler)&CBLiteTool::putDoc},
    {"query",   (FlagHandler)&CBLiteTool::queryDatabase},
    {"revs",    (FlagHandler)&CBLiteTool::revsInfo},
    {"rm",      (FlagHandler)&CBLiteTool::putDoc},
    {"select",  (FlagHandler)&CBLiteTool::queryDatabase},
    {"SELECT",  (FlagHandler)&CBLiteTool::queryDatabase},
    {"sql",     (FlagHandler)&CBLiteTool::sqlQuery},

    {"quit",    (FlagHandler)&CBLiteTool::quitCommand},

#ifdef COUCHBASE_ENTERPRISE
    {"decrypt", (FlagHandler)&CBLiteTool::decrypt},
    {"encrypt", (FlagHandler)&CBLiteTool::encrypt},
#endif
    {nullptr, nullptr}
};
