//
// CpCommand.cc
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

#include "CBLiteCommand.hh"
#include "Endpoint.hh"
#include "RemoteEndpoint.hh"
#include "DBEndpoint.hh"
#include "Stopwatch.hh"
#include "c4Private.h"
#include <optional>

using namespace std;
using namespace litecore;
using namespace fleece;


class CpCommand : public CBLiteCommand {
public:
    enum Mode {
        Cp,
        Push,
        Pull,
        Export,
        Import
    };

    CpCommand(CBLiteTool &parent, Mode mode, bool openRemote =false)
    :CBLiteCommand(parent)
    ,_mode(mode)
    ,_openRemote(openRemote)
    {
        C4RegisterBuiltInWebSocket();
    }


    ~CpCommand() {
        if (_tempDir) {
            if (_shouldCloseDB && _db) {
                C4Error err;
                bool ok = c4db_delete(_db, &err);
                _db = nullptr;
                if (!ok) {
                    cerr << "Warning: error deleting temporary database: " << err.description() << "\n";
                    cerr << "Database is in " << _tempDir->path() << "\n";
                    return;
                }
            }
            try {
                _tempDir->del();
            } catch (...) { }
        }
    }


    void usage() override {
        if (_openRemote) {
            openRemoteUsage();
            return;
        }

        cerr << ansiBold();
        if (!interactive())
            cerr << "cblite ";
        cerr << "cp" << ' ' << ansiItalic();
        cerr << "[FLAGS]" << ' ';
        if (!interactive())
            cerr << "SOURCE ";
        cerr <<
        "DESTINATION" << ansiReset() << "\n"
        "  Copies local and remote databases and JSON files.\n"
        "    --bidi : Bidirectional (push+pull) replication.\n"
        "    --cacert <file> : Use X.509 certificates in <file> to validate server TLS cert.\n"
        "    --careful : Abort on any error.\n"
        "    --cert <file> : Use X.509 certificate in <file> for TLS client authentication.\n"
        "    --continuous : Continuous replication.\n"
        "    --existing or -x : Fail if DESTINATION doesn't already exist.\n"
        "    --jsonid <property> : JSON property name to map to document IDs. (Defaults to \"_id\".)\n"
        "         * When SOURCE is JSON, this is a property name/path whose value will be used as the\n"
        "           docID. If it's not found, the document gets a UUID.\n"
        "         * When DESTINATION is JSON, this is a property name that will be added to the JSON,\n"
        "           whose value is the docID. (Set to \"\" to suppress this.)\n"
        "    --key <file> : Use private key in <file> for TLS client authentication.\n"
        "    --limit <n> : Stop after <n> documents. (Replicator ignores this)\n"
        "    --replicate : Forces use of replicator, for local-to-local db copy [EE]\n"
        "    --user <name>[:<password>] : HTTP Basic auth credentials for remote database.\n"
        "           (If password is not given, the tool will prompt you to enter it.)\n"
        "    --verbose or -v : Display progress; repeat flag for more verbosity.\n\n";

        if (interactive()) {
            cerr <<
            "  DESTINATION : Database path, replication URL, or JSON file path:\n"
            "    *.cblite2 :  Copies local database file, and assigns new UUID to target\n"
            "    *.cblite2 :  With --replicate flag, runs local replication [EE]\n"
            "    ws://*    :  Networked replication\n"
            "    wss://*   :  Networked replication, with TLS\n"
            "    *.json    :  Imports/exports JSON file (one doc per line)\n"
            "    */        :  Imports/exports directory of JSON files (one per doc)\n";

        } else {
            cerr <<
            "  SOURCE, DESTINATION : Database path, replication URL, or JSON file path:\n"
            "    *.cblite2 <--> *.cblite2 :  Copies local db file, and assigns new UUID to target\n"
            "    *.cblite2 <--> *.cblite2 :  With --replicate flag, runs local replication [EE]\n"
            "    *.cblite2 <--> ws://*    :  Networked replication\n"
            "    *.cblite2 <--> wss://*   :  Networked replication, with TLS\n"
            "    *.cblite2 <--> *.json    :  Imports/exports JSON file (one doc per line)\n"
            "    *.cblite2 <--> */        :  Imports/exports directory of JSON files (one per doc)\n";
        }

        cerr << "\n"
        "  Synonyms are \""+bold("push")+"\", \""+bold("export")+"\", \""+bold("pull")+"\", \""+bold("import")+"\".\n"
        "  * With \"pull\" and \"import\", the SOURCE and DESTINATION are reversed.\n"
        "  * \"push\" and \"pull\" always replicate, as though --replicate were given.\n"
        ;
    }


    void openRemoteUsage() {
        cerr << ansiBold();
        cerr << "cblite openremote" << ' ' << ansiItalic();
        cerr << "[FLAGS]" << ' ' << "DB_URL " << ansiReset() << "\n"
        "  Pulls a remote database to a local temporary, then starts interactive mode.\n"
        "    --bidi : Push changes back to the server (continuous mode only.)\n"
        "    --cacert <file> : Use X.509 certificates in <file> to validate server TLS cert.\n"
        "    --cert <file> : Use X.509 certificate in <file> for TLS client authentication.\n"
        "    --continuous : Continuous replication while in interactive mode.\n"
        "    --key <file> : Use private key in <file> for TLS client authentication.\n"
        "    --user <name>[:<password>] : HTTP Basic auth credentials for remote database.\n"
        "           (If password is not given, the tool will prompt you to enter it.)\n"
        "    --verbose or -v : Display progress; repeat flag for more verbosity.\n\n";
    }


    void createTemporaryDB(slice dbName) {
        if (_db)
            fail("A database is already open");
        _tempDir = FilePath(string(getenv("TMPDIR")), "").mkTempDir();
        string path = _tempDir->path();
        _dbFlags = kC4DB_Create;
        C4DatabaseConfig2 config = {slice(path), _dbFlags};
        C4Error err;
        _db = c4db_openNamed(dbName, &config, &err);
        if (!_db)
            fail("Couldn't create temporary database at " + path, err);
        _shouldCloseDB = true;
    }


    void createTemporaryDBForURL(const string &url) {
        C4Address address;
        slice dbName;
        if (!C4Address::fromURL(url, &address, &dbName))
            fail("Invalid replication URL");
        createTemporaryDB(dbName);
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--bidi",      [&]{_bidi = true;}},
            {"--careful",   [&]{_failOnError = true;}},
            {"--cert",      [&]{certFlag();}},
            {"--continuous",[&]{_continuous = true;}},
            {"--existing",  [&]{_createDst = false;}},
            {"--jsonid",    [&]{_jsonIDProperty = nextArg("JSON-id property");}},
            {"--key",       [&]{keyFlag();}},
            {"--limit",     [&]{limitFlag();}},
            {"--replicate", [&]{_replicate = true;}},
            {"--rootcerts", [&]{_rootCertsFile = nextArg("rootcerts path");}},
            {"--cacert",    [&]{_rootCertsFile = nextArg("cacert path");}}, // curl uses this name
            {"--user",      [&]{_user = nextArg("user name for replication");}},
            {"--verbose",   [&]{verboseFlag();}},
            {"-v",          [&]{verboseFlag();}},
            {"-x",          [&]{_createDst = false;}},
        });
        if (verbose() >= 2) {
            c4log_setCallbackLevel(kC4LogInfo);
            auto syncLog = c4log_getDomain("Sync", true);
            c4log_setLevel(syncLog, max(C4LogLevel(kC4LogDebug), C4LogLevel(kC4LogInfo - verbose() + 2)));
        }

        unique_ptr<Endpoint> src, dst;

        if (_openRemote) {
            string url = nextArg("remote database URL");
            createTemporaryDBForURL(url);
            src = Endpoint::create(url);
            dst = Endpoint::create(collection());

        } else {
            const char *firstArgName = "source path/URL", *secondArgName = "destination path/URL";
            if (_mode == Pull || _mode == Import)
                swap(firstArgName, secondArgName);

            try {
                src = _db ? Endpoint::create(collection())
                          : Endpoint::create(nextArg(firstArgName));
                dst = Endpoint::create(nextArg(secondArgName));
            } catch (const std::exception &x) {
                fail("Invalid endpoint: " + string(x.what()));
            }
            assert(src && dst);
            endOfArgs();

            if (_mode == Pull || _mode == Import)
                swap(src, dst);
        }

        bool dbToDb = (src->isDatabase() && dst->isDatabase());
        bool copyLocalDBs = false;

        if (_mode == Push || _mode == Pull
                || _bidi || _continuous || !_user.empty()
                || src->isRemote() || dst->isRemote())
            _replicate = true;

        if (_replicate) {
            // Set up replicator properties:
            if (_mode == Import || _mode == Export)
                failMisuse("'import' and 'export' do not support replication");
            if (!dbToDb)
                failMisuse("Replication is only possible between two databases");
            auto localDB = dynamic_cast<DbEndpoint*>(src.get());
            if (!localDB)
                localDB = dynamic_cast<DbEndpoint*>(dst.get());
            if (!localDB)
                failMisuse("Replication requires at least one database to be local");
            localDB->setBidirectional(_bidi);
            localDB->setContinuous(_continuous);

            if (!_rootCertsFile.empty())
                localDB->setRootCerts(readFile(_rootCertsFile));

            alloc_slice cert, keyData, keyPassword;
            tie(cert, keyData, keyPassword) = getCertAndKeyArgs();
            if (cert) {
                localDB->setClientCert(cert);
                localDB->setClientCertKey(keyData);
                localDB->setClientCertKeyPassword(keyPassword);
            }

            if (!_user.empty()) {
                if (cert)
                    fail("Cannot use both client cert and HTTP auth");
                string user;
                string password;
                auto colon = _user.find(':');
                if (colon != string::npos) {
                    password = _user.substr(colon+1);
                    user = _user.substr(0, colon);
                } else {
                    user = _user;
                    password = readPassword(("Server password for " + user + ": ").c_str());
                    if (password.empty())
                        exit(1);
                }
                localDB->setCredentials({user, password});
            }
        } else {
            copyLocalDBs = dbToDb;
        }

        if (_openRemote && _continuous)
            startContinuousPull(src.get(), dst.get());
        else if (copyLocalDBs)
            copyLocalToLocalDatabase((DbEndpoint*)src.get(), (DbEndpoint*)dst.get());
        else
            copyDatabase(src.get(), dst.get());

        // The `openremote` command now starts interactive mode:
        if (_openRemote) {
            CBLiteCommand::runInteractive(*this);
            if (_continuous)
                dynamic_cast<DbEndpoint*>(dst.get())->stopReplication();
        }
    }


    void copyDatabase(Endpoint *src, Endpoint *dst) {
        if (_jsonIDProperty.size == 0)
            _jsonIDProperty = nullslice;
        try {
            src->prepare(true, true, _jsonIDProperty, dst);
            dst->prepare(false, !_createDst, _jsonIDProperty, src);
        } catch (const std::exception &x) {
            fail(x.what());
        }

        Stopwatch timer;
        src->copyTo(dst, _limit);
        dst->finish();

        double time = timer.elapsed();
        cout << "Completed " << dst->docCount() << " docs in " << time << " secs; "
             << int(dst->docCount() / time) << " docs/sec\n";
        if (_errorCount > 0)
            cerr << "** " << _errorCount << " errors occurred; see above **\n";
    }


    void copyLocalToLocalDatabase(DbEndpoint *src, DbEndpoint *dst) {
        auto [dstDir, dstName] = CBLiteTool::splitDBPath(string(dst->path()));
        if (dstName.empty())
            fail("Database filename must have a '.cblite2' extension");
        if (verbose())
            cout << "Copying to " << dst->path() << " ...\n";

        C4DatabaseConfig2 config = {slice(dstDir), kC4DB_Create | kC4DB_AutoCompact };

        Stopwatch timer;
        C4Error error;
        if (!c4db_copyNamed(src->path(), slice(dstName), &config, &error))
            errorOccurred("copying database", error);
        double time = timer.elapsed();
        cout << "Completed copy in " << time << " secs\n";
    }


    void startContinuousPull(Endpoint *src, Endpoint *dst) {
        try {
            src->prepare(true,  true, nullslice, dst);
            dst->prepare(false, true, nullslice, src);
        } catch (const std::exception &x) {
            fail(x.what());
        }

        auto oldVerbose = CBLiteTool::instance()->verbose();
        CBLiteTool::instance()->setVerbose(1);

        auto localDB = dynamic_cast<DbEndpoint*>(dst);
        auto remoteDB = dynamic_cast<RemoteEndpoint*>(src);
        assert(localDB);
        assert(remoteDB);
        localDB->startReplicationWith(*remoteDB, false);
        localDB->waitTillIdle();
        cerr << "Local database is ready. Replicator will remain active to sync any later changes.\n";

        CBLiteTool::instance()->setVerbose(oldVerbose);
    }


    void pullRemoteDatabase(const string &url) {
        auto oldVerbose = CBLiteTool::instance()->verbose();
        CBLiteTool::instance()->setVerbose(1);

        auto src = Endpoint::createRemote(url);
        auto dst = Endpoint::create(_db);
        copyDatabase(src.get(), dst.get());
        
        CBLiteTool::instance()->setVerbose(oldVerbose);
    }

private:
    Mode const              _mode;
    bool                    _createDst {true};
    bool                    _bidi {false};
    bool                    _continuous {false};
    bool                    _replicate {false};
    bool                    _openRemote {false};
    alloc_slice             _jsonIDProperty {"_id"};
    std::string             _rootCertsFile;
    string                  _user;
    optional<FilePath>      _tempDir;
};


tuple<alloc_slice, alloc_slice, alloc_slice> CBLiteCommand::getCertAndKeyArgs() {
    if (_certFile.empty())
        return {};
    alloc_slice cert = readFile(_certFile);

    if (_keys.empty())
        fail("TLS cert given but no key; use --key KEYFILE");
    string keyFile(*_keys.begin());
    alloc_slice keyData = readFile(keyFile);
    alloc_slice keyPassword;
    if (keyData.containsBytes("-----BEGIN ENCRYPTED "_sl))
        keyPassword = alloc_slice(readPassword("Private key password: "));
    return {cert, keyData, keyPassword};
}


CBLiteCommand* newCpCommand(CBLiteTool &parent) {
    return new CpCommand(parent, CpCommand::Cp);
}
CBLiteCommand* newPushCommand(CBLiteTool &parent) {
    return new CpCommand(parent, CpCommand::Push);
}
CBLiteCommand* newPullCommand(CBLiteTool &parent) {
    return new CpCommand(parent, CpCommand::Pull);
}
CBLiteCommand* newExportCommand(CBLiteTool &parent) {
    return new CpCommand(parent, CpCommand::Export);
}
CBLiteCommand* newImportCommand(CBLiteTool &parent) {
    return new CpCommand(parent, CpCommand::Import);
}

CBLiteCommand* newOpenRemoteCommand(CBLiteTool &parent) {
    return new CpCommand(parent, CpCommand::Pull, true);
}


void CBLiteCommand::runInteractiveWithURL(CBLiteTool &parent, const string &databaseURL) {
    CpCommand cmd(parent, CpCommand::Pull, true);
    cmd.createTemporaryDBForURL(databaseURL);
    cmd.setVerbose(1);
    cmd.pullRemoteDatabase(databaseURL);
    CBLiteCommand::runInteractive(cmd);
}
