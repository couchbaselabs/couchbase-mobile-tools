//
// LiteServ.cc
//
// Copyright © 2024 Couchbase. All rights reserved.
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

#include "LiteCoreTool.hh"
#include "StringUtil.hh"            // for digittoint(), on non-BSD-like systems
#include "c4Certificate.h"
#include "c4Private.h"
#include "fleece/FLExpert.h"        // for FLJSON5_ToJSON()
#include <signal.h>
#include <thread>

using namespace std;
using namespace litecore;
using namespace fleece;


static constexpr int kDefaultPort = 59840;


class LiteServTool : public LiteCoreTool {
public:
    static inline auto sRESTLog = c4log_getDomain("REST", true);
    static inline int sVerboseLog;

    LiteServTool() :LiteCoreTool("LiteServ") { }

    void usage() override {
        cerr <<
        ansiBold() << "liteserv: Simple Couchbase Lite database server\n\n" << ansiReset() <<
        bold("Usage:\n") <<
        "  liteserv " << it("[FLAGS] /path/to/db.cblite2") << "\n" <<
        "  liteserv --config " << it("CONFIGFILE") << "\n" <<
        bold("Flags") << "\n"
        "  --config CONFIGFILE  : Read configuration from JSON file; overrides other flags.\n"
        "  --create             : Create the database if it doesn't already exist.\n"
        "  --writeable          : Allows REST calls to modify the database\n"
        "  --port N             : TCP port number (default "<<kDefaultPort<<")\n"
#ifdef COUCHBASE_ENTERPRISE
        "  --cert FILE          : Path to X.509 certificate file, for TLS\n"
        "  --key FILE           : Path to private key file, for TLS\n"
        "  --client-cert FILE   : Path to file containing root cert for TLS client auth\n"
#endif
        "  --verbose or -v      : Show LiteCore log messages\n"
        "  --version            : Display version info and exit\n\n"
        << bold("Note:") << " Only a subset of the Sync Gateway / CouchDB REST API is implemented so far.\n"
        "      See <https://github.com/couchbase/couchbase-lite-core/wiki/REST-API>\n"
#if defined(COUCHBASE_ENTERPRISE) && defined(__APPLE__)
        << bold("Note:") << " On macOS, the value of --cert may be the name of a certificate in the Keychain.\n"
#endif
        << "\n" << bold("Config file example:") << "\n"
        "    { interface: \"0.0.0.0:59840\",\n"
        "      https: { tls_cert_path: \"…\", tls_key_path: \"…\", tls_client_cert_path: \"…\"},  // optional\n"
        "      databases: {\n"
        "        foo: { path: \"/path/to/db.cblite2\", read_only: true }\n"
        "      } }\n"
        ;
    }


    int run() override {
        initialize();
        assert(_listener);

    #ifndef _MSC_VER
        // Run until the process receives SIGINT (^C) or SIGHUP:
        static bool gStop;
        gStop = false;
        struct sigaction action = {{[](int s) {gStop = true;}}, 0, SA_RESETHAND};
        sigaction(SIGHUP, &action, nullptr);
        sigaction(SIGINT, &action, nullptr);
        while(!gStop)
            this_thread::sleep_for(chrono::seconds(1));

        c4log(sRESTLog, kC4LogInfo, "Stopped.");
        _listener = nullptr;
    #endif
        return 0;
    }


    void initialize() {
        // Default configuration (everything else is false/zero):
        _listenerConfig.port = kDefaultPort;
        _listenerConfig.apis = kC4RESTAPI;

        string configFile;
#ifdef COUCHBASE_ENTERPRISE
        string certFile, keyFile, clientCertFile;
#endif

        // Read args:
        processFlags({
            {"--config",        [&]{configFile = nextArg("config file path");}},
            {"--create",        [&]{_dbFlags |= kC4DB_Create; _dbFlags &= ~kC4DB_ReadOnly;}},
            {"--writeable",     [&]{_dbFlags &= ~kC4DB_ReadOnly;}},
            {"--encrypted",     [&]{_dbNeedsPassword = true;}},
            {"--version",       [&]{displayVersion();}},
            {"--verbose",       [&]{verboseFlag();}},
            {"-v",              [&]{verboseFlag();}},
            {"--port",          [&]{_listenerConfig.port = parseNextArg<uint16_t>("port");}},
#ifdef COUCHBASE_ENTERPRISE
            {"--cert",          [&]{certFile = nextArg("certificate path");}},
            {"--key",           [&]{keyFile = nextArg("key path");}},
            {"--client-cert",   [&]{clientCertFile = nextArg("certificate path");}},
#endif
        });

        // Set up logging levels:
        sVerboseLog = _verbose;
        c4log_writeToCallback(kC4LogInfo, writeLogMessage, true);

#ifdef COUCHBASE_ENTERPRISE
        initializeTLS(certFile, keyFile, clientCertFile);
#endif

        if (!configFile.empty()) {
            endOfArgs();
            initializeFromConfigFile(configFile);
        } else {
            openDatabase(nextArg("database path"), false);
            endOfArgs();
            startListener();
            shareDB(_db);
        }
    }


    void initializeFromConfigFile(string const& configPath) {
        FLStringResult errorMessage {};
        size_t errorPos;
        alloc_slice json(FLJSON5_ToJSON(readFile(configPath), &errorMessage, &errorPos, nullptr));
        if (!json)
            fail(stringprintf("Couldn't parse config file as JSON5: %.*s", FMTSLICE(errorMessage)));
        Doc configDoc = Doc::fromJSON(json);
        Dict config = configDoc.asDict();
        if (!config) fail("Couldn't read config file; contents must be a JSON object");

        if (Value intfV = config["interface"]) {
            auto [addr, portStr] = split2(intfV.asString(), ":");
            if (!addr.empty()) {
                _networkInterface = alloc_slice(addr);
                _listenerConfig.networkInterface = _networkInterface;
            }
            _listenerConfig.port = parse<uint16_t>("interface", portStr);
        }

        if (Value tlsV = config["https"]) {
#ifdef COUCHBASE_ENTERPRISE
            if (_listenerConfig.tlsConfig != nullptr)
                fail("Config file's 'https' property conflicts with command-line --cert flag");
            Dict tls = tlsV.asDict();
            initializeTLS(tls["tls_cert_path"].asstring(), tls["tls_key_path"].asstring(),
                          tls["tls_client_cert_path"].asstring());
#else
            fail("The 'https' property in the config file is not supported: TLS is an Enterprise Edition feature.");
#endif
        }

        Dict dbs = config["databases"].asDict();
        if (dbs.empty())
            fail("config file must have a 'databases' object");

        startListener();

        for (Dict::iterator i(dbs); i; ++i) {
            // Open each database:
            slice name = i.keyString();
            Dict dbConfig = i->asDict();
            string path = dbConfig["path"].asstring();
            if (path.empty()) fail("missing 'path' in database config");
            _dbFlags = {};
            if (dbConfig["read_only"].asBool())
                _dbFlags |= kC4DB_ReadOnly;
            _db = nullptr;
            openDatabase(path, false);
            shareDB(_db, name);
        }
    }

private:

    void startListener() {
        if (_listener)
            return;

        // Start the listener:
        C4RegisterBuiltInWebSocket();
        C4Error err;
        _listener = c4listener_start(&_listenerConfig, &err);
        if (!_listener)
            fail("starting REST listener", err);
    }


    void shareDB(C4Database* db, slice asName = {}) {
        alloc_slice path(c4db_getPath(db));
        alloc_slice dbName;
        if (asName.empty())
            dbName = alloc_slice(c4db_URINameFromPath(path));   // default name is file's base name
        else
            dbName = asName;

        startListener();
        if (C4Error error; !c4listener_shareDB(_listener, dbName, db, &error)) {
            fail(stringprintf("Unable to share database at %.*s: %s",
                              FMTSLICE(path), error.description().c_str()));
        }

        // Announce the URL:
        c4log(sRESTLog, kC4LogInfo, "Sharing database at %s://localhost:%u/%.*s/",
              (_listenerConfig.tlsConfig ? "https" : "http"),
              _listenerConfig.port,
              FMTSLICE(dbName));
    }


    static void writeLogMessage(C4LogDomain domain, C4LogLevel level, const char* msg, va_list) {
        if (domain == sRESTLog || level >= kC4LogWarning || sVerboseLog > 0) {
            ostream& out = (level >= kC4LogWarning) ? cerr : cout;
            char dateStr[100];
            time_t t = time(nullptr);
            strftime(dateStr, sizeof(dateStr), "%F %T    ", localtime(&t));
            out << dateStr;

            switch (level) {
                case kC4LogWarning: out << "WARNING: "; break;
                case kC4LogError:   out << "ERROR: "; break;
                default: break;
            }
            if (domain != sRESTLog)
                cout << '(' << c4log_getDomainName(domain) << ") ";
            out << msg << endl;
        }
    }


#ifdef COUCHBASE_ENTERPRISE
    void initializeTLS(string const& certFile, string const& keyFile, string const& clientCertFile) {
        if (!certFile.empty()) {
            _tlsConfig = makeTLSConfig(certFile, keyFile, clientCertFile);
            _listenerConfig.tlsConfig = &_tlsConfig;
            cout << "Using server TLS certificate: " << certName(_tlsConfig.certificate) << endl;
            if (_tlsConfig.rootClientCerts) {
                cout << "Requiring client TLS certificate: " << certName(_tlsConfig.rootClientCerts) << endl;
            }
        } else {
            if (!keyFile.empty())
                fail("TLS key given but no cert; use --cert CERTFILE");
            if (!clientCertFile.empty())
                fail("TLS client cert given but no server cert; use --cert CERTFILE --key KEYFILE");
        }
    }

    string certName(C4Cert* cert) {
        alloc_slice name(_verbose ? c4cert_summary(cert) : c4cert_subjectName(cert));
        string str(name);
        if (_verbose) str.insert(0, "\n");
        return str;
    }
#endif


    C4ListenerConfig    _listenerConfig {};  // all false/0
    alloc_slice         _networkInterface;
    c4::ref<C4Listener> _listener;
#ifdef COUCHBASE_ENTERPRISE
    TLSConfig           _tlsConfig {};
#endif
};


int main(int argc, const char* argv[]) {
    LiteServTool tool;
    return tool.main(argc, argv);
}


