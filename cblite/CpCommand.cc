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
#include "DBEndpoint.hh"
#include "Stopwatch.hh"
#include "c4Private.h"


class CpCommand : public CBLiteCommand {
public:
    enum Mode {
        Cp,
        Push,
        Pull,
        Export,
        Import
    };

    CpCommand(CBLiteTool &parent, Mode mode)
    :CBLiteCommand(parent)
    ,_mode(mode)
    { }


    void usage() override {
        cerr << ansiBold();
        if (!_interactive)
            cerr << "cblite ";
        cerr << "cp" << ' ' << ansiItalic();
        cerr << "[FLAGS]" << ' ';
        if (!_interactive)
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
        "    --key <file> : Use private key in <file> for TLS client authentication."
        "    --limit <n> : Stop after <n> documents. (Replicator ignores this)\n"
        "    --replicate : Forces use of replicator, for local-to-local db copy [EE]\n"
        "    --user <name>[:<password>] : HTTP Basic auth credentials for remote database.\n"
        "           (If password is not given, the tool will prompt you to enter it.)\n"
        "    --verbose or -v : Display progress; repeat flag for more verbosity.\n";

        if (_interactive) {
            cerr <<
            "  DESTINATION : Database path, replication URL, or JSON file path:\n"
            "    *.cblite2 :  Copies local database file, and assigns new UUID to target\n"
            "    *.cblite2 :  With --replicate flag, runs local replication [EE]\n"
            "    ws://*    :  Networked replication\n"
            "    wss://*   :  Networked replication, with TLS\n"
            "    *.json    :  Imports/exports JSON file (one doc per line)\n"
            "    */        :  Imports/exports directory of JSON files (one per doc)\n";
            cerr <<
            "  Synonyms are \""+bold("push")+"\", \""+bold("export")+"\", \""+bold("pull")+"\", \""+bold("import")+"\".\n"
            "  With \"pull\" and \"import\", the parameter is the SOURCE while the current database\n"
            "  is the DESTINATION.\n"
            "  \"push\" and \"pull\" always replicate, as though --replicate were given.\n"
            ;

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
    }


    void runSubcommand() override {
        // Register built-in WebSocket implementation:
        C4RegisterBuiltInWebSocket();

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
            c4log_setLevel(syncLog, max(kC4LogDebug, C4LogLevel(kC4LogInfo - verbose() + 2)));
        }

        const char *firstArgName = "source path/URL", *secondArgName = "destination path/URL";
        if (_mode == Pull || _mode == Import)
            swap(firstArgName, secondArgName);

        unique_ptr<Endpoint> src(_db ? Endpoint::create(_db)
                                     : Endpoint::create(nextArg(firstArgName)));
        unique_ptr<Endpoint> dst(Endpoint::create(nextArg(secondArgName)));
        if (!src || !dst)
            fail("Invalid endpoint");
        if (hasArgs())
            fail(format("Too many arguments, starting with `%s`", peekNextArg().c_str()));

        if (_mode == Pull || _mode == Import)
            swap(src, dst);

        bool dbToDb = (src->isDatabase() && dst->isDatabase());
        bool copyLocalDBs = false;

        if (_mode == Push || _mode == Pull
                || _bidi || _continuous || !_user.empty()
                || src->isRemote() || dst->isRemote())
            _replicate = true;
        if (_replicate) {
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

        if (copyLocalDBs)
            copyLocalToLocalDatabase((DbEndpoint*)src.get(), (DbEndpoint*)dst.get());
        else
            copyDatabase(src.get(), dst.get());
    }


    void copyDatabase(Endpoint *src, Endpoint *dst) {
        if (_jsonIDProperty.size == 0)
            _jsonIDProperty = nullslice;
        src->prepare(true, true, _jsonIDProperty, dst);
        dst->prepare(false, !_createDst, _jsonIDProperty, src);

        Stopwatch timer;
        src->copyTo(dst, _limit);
        dst->finish();

        double time = timer.elapsed();
        cout << "Completed " << dst->docCount() << " docs in " << time << " secs; "
             << int(dst->docCount() / time) << " docs/sec\n";
    }


    void copyLocalToLocalDatabase(DbEndpoint *src, DbEndpoint *dst) {
        alloc_slice dstPath = dst->path();
        if (verbose())
            cout << "Copying to " << dstPath << " ...\n";

        C4DatabaseConfig config = { kC4DB_Create | kC4DB_AutoCompact | kC4DB_SharedKeys };

        Stopwatch timer;
        C4Error error;
        if (!c4db_copy(src->path(), dstPath, &config, &error))
            Tool::instance->errorOccurred("copying database", error);
        double time = timer.elapsed();
        cout << "Completed copy in " << time << " secs\n";
    }

private:
    Mode const              _mode;
    bool                    _createDst {true};
    bool                    _bidi {false};
    bool                    _continuous {false};
    bool                    _replicate {false};
    alloc_slice             _jsonIDProperty {"_id"};
    std::string             _rootCertsFile;
    string                  _user;
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
