//
// CBLiteTool.cc
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

#include "CBLiteTool.hh"
#include "CBLiteCommand.hh"
#include "StringUtil.hh"            // for digittoint(), on non-BSD-like systems

using namespace litecore;
using namespace std;
using namespace fleece;

int main(int argc, const char * argv[]) {
    CBLiteTool tool;
    return tool.main(argc, argv);
}


CBLiteTool::~CBLiteTool() {
    if (_shouldCloseDB && _db) {
        C4Error err;
        bool ok = c4db_close(_db, &err);
        if (!ok)
            cerr << "Warning: error closing database: " << c4error_descriptionStr(err) << "\n";
    }
}


void CBLiteTool::usage() {
    cerr <<
    ansiBold() << "cblite: Couchbase Lite / LiteCore database multi-tool\n\n" << ansiReset() <<
    bold("Usage:\n") <<
    "  cblite " << it("[FLAGS] SUBCOMMAND /path/to/db.cblite2 ...") << "\n"
    "  cblite " << it("[FLAGS] /path/to/db.cblite2 ...") << "       (interactive mode*)\n"
    "  cblite " << it("[FLAGS] wss://host:port/dbname ...") << "    (interactive mode*)\n"
    "\n" <<
    bold("Global Flags") << " (before the subcommand name):\n"
    "  --color     : Use bold/italic (sometimes color), if terminal supports it\n"
    "  --create    : Create the database if it doesn't already exist.\n"
#ifdef COUCHBASE_ENTERPRISE
    "  --encrypted : Open an encrypted database (prompts for password)\n"
#endif
    "  --upgrade   : Allow DB version upgrade (breaking backward compatibility)\n"
    "  --upgrade=vv: Upgrade DB from rev-trees to version vectors (experimental! irreversible!)\n"
    "  --version   : Display version info and exit\n"
    "  --writeable : Open the database with read+write access\n"
    "\n" <<
    bold("Subcommands:\n") <<
    "    cat, get       : display document body(ies) as JSON\n"
    "    cd             : set the current collection\n"
    "    check          : check for database corruption\n"
    "    compact        : free up unused space\n"
    "    edit           : update or create a document as JSON in a text editor\n"
#ifdef COUCHBASE_ENTERPRISE
    "    encrypt,decrypt: add or remove encryption\n"
#endif
    "    help           : print more help for a subcommand\n"
    "    import, export : copy to/from JSON files\n"
    "    info, file     : information & stats about the database\n"
    "    ls             : list the IDs of documents in the database\n"
    "    lscoll         : list the collections in the database\n"
    "    mkcoll         : create a collection\n"
    "    mkindex        : create an index\n"
    "    mv             : move documents from one collection to another\n"
    "    open           : open DB and start interactive mode*\n"
    "    openremote     : pull remote DB to temp file & start interactive mode*\n"
    "    push, pull     : replicate to/from a remote database\n"
    "    put            : create or modify a document\n"
    "    query, select  : run a N1QL or JSON query\n"
    "    reindex        : drop and recreates an index\n"
    "    revs           : show the revisions of a document\n"
    "    rm             : delete documents\n"
    "    rmindex        : delete an index\n"
    "    serve          : start a simple REST server on the DB\n"
    "\n"
    "    Most subcommands take their own flags or parameters, following the name.\n"
    "    For details, run `cblite help " << it("SUBCOMMAND") << "`.\n"
    "\n" <<
    bold("Interactive Mode:\n") <<
    "  * The interactive 'shell' displays a `(cblite)` prompt and lets you enter a subcommand.\n"
    "    Since the database is already open, you don't have to give its path again, nor any of\n"
    "    the global flags, simply the subcommand and any flags or parameters it takes.\n"
    "    For example: `cat doc123`, `ls -l`, `help push`.\n"
    "    Exit the shell by entering `quit` or pressing Ctrl-D (on Unix) or Ctrl-Z (on Windows).\n\n"
    "Online docs: " << ansiUnderline() <<
    "https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/Documentation.md"
    << ansiReset() << endl;
}


void CBLiteTool::displayVersion() {
    alloc_slice version = c4_getVersion();
    cout << "Couchbase Lite Core " << version << "\n";
    ::exit(0);
}


int CBLiteTool::run() {
    // Initial pre-subcommand flags:
    processFlags({
        {"--create",    [&]{_dbFlags |= kC4DB_Create; _dbFlags &= ~kC4DB_ReadOnly;}},
        {"--writeable", [&]{_dbFlags &= ~kC4DB_ReadOnly;}},
        {"--upgrade",   [&]{_dbFlags &= ~(kC4DB_NoUpgrade | kC4DB_ReadOnly);}},
#if LITECORE_API_VERSION >= 300
        {"--upgrade=vv",[&]{_dbFlags &= ~(kC4DB_NoUpgrade | kC4DB_ReadOnly);
                            _dbFlags |= kC4DB_VersionVectors;}},
#endif
        {"--encrypted", [&]{_dbNeedsPassword = true;}},
        {"--version",   [&]{displayVersion();}},
        {"-v",          [&]{displayVersion();}},
    });

    c4log_setCallbackLevel(kC4LogWarning);
    if (!hasArgs()) {
        cerr << ansiBold()
             << "cblite: Couchbase Lite / LiteCore database multi-tool\n" << ansiReset() 
             << "ERROR: Missing subcommand or database path.\n"
             << "For a list of subcommands, run " << ansiBold() << "cblite help" << ansiReset() << ".\n"
             << "To start the interactive mode, run "
             << ansiBold() << "cblite " << ansiItalic() << "/path/of/db.cblite2" << ansiReset() << '\n';
        fail();
    }

    string cmd = nextArg("subcommand or database path");
    if (isDatabaseURL(cmd)) {
        // Interactive mode on remote URL:
        endOfArgs();
        CBLiteCommand::runInteractiveWithURL(*this, cmd);
    } else if (isDatabasePath(cmd)) {
        // Interactive mode:
        endOfArgs();
        CBLiteCommand::runInteractive(*this, cmd);
    } else if (cmd == "help") {
        // Run "help" subcommand:
        helpCommand();
    } else {
        // Run subcommand:
        auto subcommandInstance = subcommand(cmd);
        if (subcommandInstance) {
            subcommandInstance->runSubcommand();
        } else {
            if (cmd.find(FilePath::kSeparator) != string::npos || cmd.find('.') || cmd.size() > 10)
                fail(format("Not a valid database path (must end in %s) or subcommand name: %s",
                            kC4DatabaseFilenameExtension, cmd.c_str()));
            else
                failMisuse(format("Unknown subcommand '%s'", cmd.c_str()));
        }
    }
    return 0;
}


#pragma mark - OPENING DATABASE:


bool CBLiteTool::isDatabasePath(const string &path) {
    return ! splitDBPath(path).second.empty();
}


pair<string,string> CBLiteTool::splitDBPath(const string &pathStr) {
    FilePath path(pathStr);
    if (path.extension() != kC4DatabaseFilenameExtension)
        return {"",""};
    return std::make_pair(string(path.parentDir()), path.unextendedName());
}


bool CBLiteTool::isDatabaseURL(const string &str) {
    C4Address addr;
    C4String dbName;
    return c4address_fromURL(slice(str), &addr, &dbName);
}


#if COUCHBASE_ENTERPRISE
static bool setHexKey(C4EncryptionKey *key, const string &str) {
    if (str.size() != 2 * kC4EncryptionKeySizeAES256)
        return false;
    uint8_t *dst = &key->bytes[0];
    for (size_t src = 0; src < 2 * kC4EncryptionKeySizeAES256; src += 2) {
        if (!isxdigit(str[src]) || !isxdigit(str[src+1]))
            return false;
        *dst++ = (uint8_t)(16*digittoint(str[src]) + digittoint(str[src+1]));
    }
    key->algorithm = kC4EncryptionAES256;
    return true;
}
#endif


void CBLiteTool::openDatabase(string pathStr, bool interactive) {
    fixUpPath(pathStr);
    auto [parentDir, dbName] = splitDBPath(pathStr);
    if (dbName.empty())
        fail("Database filename must have a '.cblite2' extension");
    C4DatabaseConfig2 config = {slice(parentDir), _dbFlags};
    C4Error err;
    const C4Error kEncryptedDBError = {LiteCoreDomain, kC4ErrorNotADatabaseFile};

    if (const char* extPath = getenv("CBLITE_EXTENSION_PATH")) {
        c4_setExtensionPath(slice(extPath));
    }

    if (!_dbNeedsPassword) {
        _db = c4db_openNamed(slice(dbName), &config, &err);
    } else {
        // If --encrypted flag given, skip opening db as unencrypted
        err = kEncryptedDBError;
    }

    while (!_db && err == kEncryptedDBError) {
#ifdef COUCHBASE_ENTERPRISE
        // Database is encrypted
        if (!interactive && !_dbNeedsPassword) {
            // Don't prompt for a password unless this is an interactive session
            fail("Database is encrypted (use `--encrypted` flag to get a password prompt)");
        }
        const char *prompt = "Database password or hex key:";
        if (config.encryptionKey.algorithm != kC4EncryptionNone)
            prompt = "Sorry, try again: ";
        string password = readPassword(prompt);
        if (password.empty())
            exit(1);
        if (!setHexKey(&config.encryptionKey, password)
                && !c4key_setPassword(&config.encryptionKey, slice(password), kC4EncryptionAES256)) {
            cout << "Error: Couldn't derive key from password\n";
            continue;
        }
        _db = c4db_openNamed(slice(dbName), &config, &err);
        if (!_db && err == kEncryptedDBError) {
            cout << "Failed to decrypt database using current method, trying old method..." << endl;
            if (!c4key_setPasswordSHA1(&config.encryptionKey, slice(password), kC4EncryptionAES256)) {
                cout << "Error: Couldn't derive key from password\n";
                continue;
            }

            _db = c4db_openNamed(slice(dbName), &config, &err);
        }
#else
        fail("Database is encrypted (Enterprise Edition is required to open encrypted databases)");
#endif
    }
    
    if (!_db) {
        if (err.domain == LiteCoreDomain && err.code == kC4ErrorCantUpgradeDatabase
                && (_dbFlags & kC4DB_NoUpgrade)) {
            fail("The database needs to be upgraded to be opened by this version of LiteCore.\n"
                 "**This will likely make it unreadable by earlier versions.**\n"
                 "To upgrade, add the `--upgrade` flag before the database path.\n"
                 "(Detailed error message", err);
        }
        fail(format("Couldn't open database %s", pathStr.c_str()), err);
    }
    _shouldCloseDB = true;
}


#pragma mark - COMMANDS:


void CBLiteTool::helpCommand() {
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


using ToolFactory = CBLiteCommand* (*)(CBLiteTool&);

static constexpr struct {const char* name; ToolFactory factory;} kSubcommands[] = {
    {"cat",     newCatCommand},
    {"check",   newCheckCommand},
    {"compact", newCompactCommand},
    {"cp",      newCpCommand},
    {"edit",    newEditCommand},
    {"enrich",  newEnrichCommand},
    {"export",  newExportCommand},
    {"file",    newInfoCommand},
    {"get",     newCatCommand},
    {"import",  newImportCommand},
    {"info",    newInfoCommand},
    {"ls",      newListCommand},
    {"lscoll",  newListCollectionsCommand},
    {"mkindex", newMkIndexCommand},
    {"open",    newOpenCommand},
    {"openremote", newOpenRemoteCommand},
    {"pull",    newPullCommand},
    {"push",    newPushCommand},
    {"put",     newPutCommand},
    {"query",   newQueryCommand},
    {"reindex", newReindexCommand},
    {"revs",    newRevsCommand},
    {"rm",      newRmCommand},
    {"rmindex", newRmIndexCommand},
    {"SELECT",  newSelectCommand},
    {"select",  newSelectCommand},
    {"sql",     newSQLCommand},
    {"serve",   newServeCommand},
    {"cd",      newCdCommand},
    {"mkcoll",  newMkCollCommand},
    {"mv",      newMvCommand},
#ifdef COUCHBASE_ENTERPRISE
    {"decrypt", newDecryptCommand},
    {"encrypt", newEncryptCommand},
#endif
};


unique_ptr<CBLiteCommand> CBLiteTool::subcommand(const string &name) {
    for (const auto &entry : kSubcommands) {
        if (name == entry.name) {
            unique_ptr<CBLiteCommand> command( entry.factory(*this) );
            command->setName(name);
            return command;
        }
    }
    return nullptr;
}


void CBLiteTool::addLineCompletions(ArgumentTokenizer &tokenizer,
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
