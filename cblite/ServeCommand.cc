//
// ServeCommand.cc
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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
#include "c4Private.h"
#include <signal.h>
#include <thread>
using namespace std;
using namespace litecore;

static bool gStop = false;


class ServeCommand : public CBLiteCommand {
public:
    static constexpr int kDefaultPort = 59840;

    
    ServeCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("serve", true);
        if (!interactive()) {
            cerr << ansiBold() << "cblite serve " << ansiItalic() << "[FLAGS] --dir DIRECTORY"
                 << ansiReset() << "\n";
        }
        cerr <<
    #ifdef COUCHBASE_ENTERPRISE
        "  Runs a REST API and sync server\n"
        "    --cert CERTFILE : Path to X.509 certificate file, for TLS\n"
        "    --key KEYFILE : Path to private key file, for TLS\n"
    #else
        "  Runs a REST API server\n"
    #endif
        "    --port N : Sets TCP port number (default "<<kDefaultPort<<")\n"
        "    --readonly : Prevents REST calls from altering the database\n"
        "    --verbose or -v : Logs requests; repeat flag for more verbosity\n"
        "  Note: Only a subset of the Couchbase Lite REST API is implemented so far.\n"
        "        See <github.com/couchbase/couchbase-lite-core/wiki/REST-API>\n"
        ;
    }


    static alloc_slice databaseNameFromPath(slice path) {
        return alloc_slice(c4db_URINameFromPath(path));
    }


    void startListener() {
        if (!_listener) {
            C4Error err;
            _listener = c4listener_start(&_listenerConfig, &err);
            if (!_listener)
                fail("starting REST listener", err);
        }
    }


    void runSubcommand() override {
        // Register built-in WebSocket implementation:
        C4RegisterBuiltInWebSocket();

        // Default configuration (everything else is false/zero):
        _listenerConfig.port = kDefaultPort;
        _listenerConfig.apis = c4listener_availableAPIs();
        _listenerConfig.allowPush = true;

        // Unlike other subcommands, this one opens the db read/write, unless --readonly is specified
        _dbFlags = _dbFlags & ~kC4DB_ReadOnly;

        // Read params:
        processFlags({
#ifdef COUCHBASE_ENTERPRISE
            {"--cert",      [&]{certFlag();}},
#endif
            {"--dir",       [&]{_listenerDirectory = nextArg("directory");}},
            {"--key",       [&]{keyFlag();}},
            {"--port",      [&]{_listenerConfig.port = parseNextArg<uint16_t>("port");}},
            {"--readonly",  [&]{_dbFlags = (_dbFlags | kC4DB_ReadOnly) & ~kC4DB_Create;}},
            {"--verbose",   [&]{verboseFlag();}},
            {"-v",          [&]{verboseFlag();}},
        });
#ifdef COUCHBASE_ENTERPRISE
        C4TLSConfig tlsConfig = {};
        alloc_slice certData, keyData, keyPassword;
        tie(certData, keyData, keyPassword) = getCertAndKeyArgs();
        if (certData) {
            C4Error err;
            c4::ref<C4Cert> cert = c4cert_fromData(certData, &err);
            if (!cert)
                fail("Couldn't read certificate data", err);
            c4::ref<C4KeyPair> key = c4keypair_fromPrivateKeyData(keyData, keyPassword, &err);
            if (!key)
                fail("Couldn't read private key data", err);
            tlsConfig.certificate = cert;
            tlsConfig.key = key;
            tlsConfig.privateKeyRepresentation = kC4PrivateKeyFromKey;
            _listenerConfig.tlsConfig = &tlsConfig;
        }
#endif

        bool serveDirectory = !_listenerDirectory.empty();
        if (serveDirectory) {
            if (_db)
                fail("--dir flag cannot be used in interactive mode");
            _listenerConfig.directory = slice(_listenerDirectory);
        }

        if (!(_dbFlags & kC4DB_ReadOnly)) {
            _listenerConfig.allowPull = true;
            if (serveDirectory)
                _listenerConfig.allowCreateDBs = _listenerConfig.allowDeleteDBs = true;
        }

        if (!serveDirectory)
            openDatabaseFromNextArg();
        endOfArgs();

        c4log_setCallbackLevel(kC4LogInfo);
        auto restLog = c4log_getDomain("REST", true);
        c4log_setLevel(restLog, max(C4LogLevel(kC4LogDebug), C4LogLevel(kC4LogInfo - verbose())));
        restLog = c4log_getDomain("Listener", true);
        c4log_setLevel(restLog, kC4LogInfo);

        startListener();

        alloc_slice name;
        if (_db) {
            alloc_slice dbPath(c4db_getPath(_db));
            name = databaseNameFromPath(dbPath);
            C4Error err;
            bool started = c4listener_shareDB(_listener, name, _db, &err);
            if(!started) {
                cerr << "Got error when starting listener: " << err.domain << " / " << err.code << endl;
                fail("Failed to start listener...");
            }
        }

        // Announce the URL(s):
        string urlSuffix = CONCAT((_listenerConfig.tlsConfig ? "s" : "")
                                << "://localhost:" << _listenerConfig.port << "/");
        if (!serveDirectory)
            urlSuffix += string(name) + "/";
        if (_listenerConfig.apis & kC4RESTAPI) {
            cout << "LiteCore REST server is now listening at " << ansiBold() << ansiUnderline()
                 << "http" << urlSuffix << ansiReset() << "\n";
        }
        if (_listenerConfig.apis & kC4SyncAPI) {
            cout << "LiteCore sync server is now listening at " << ansiBold() << ansiUnderline()
                << "ws" << urlSuffix;
            if (!serveDirectory)
                cout << "_blipsync";
            cout << ansiReset() << "\n";
        }

    #ifndef _MSC_VER
        // Run until the process receives SIGINT (^C) or SIGHUP:
        struct sigaction action = {{[](int s) {gStop = true;}}, 0, SA_RESETHAND};
        sigaction(SIGHUP, &action, nullptr);
        sigaction(SIGINT, &action, nullptr);
        cout << it("(Press ^C to stop)\n");
        while(!gStop)
            this_thread::sleep_for(chrono::seconds(1));
    #else
        // TODO: Use SetConsoleCtrlHandler() to do something like the above, on Windows.
        cout << it("Press Enter to stop server: ");
        (void)getchar();
    #endif

        cout << " Stopping server...\n";
        c4listener_free(_listener);
        _listener = nullptr;
    }

private:
    std::string             _listenerDirectory;
    C4ListenerConfig        _listenerConfig {};  // all false/0
    c4::ref<C4Listener>     _listener;
};


CBLiteCommand* newServeCommand(CBLiteTool &parent) {
    return new ServeCommand(parent);
}
