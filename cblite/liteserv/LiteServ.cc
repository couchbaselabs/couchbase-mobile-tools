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
        "  liteserv " << it("[FLAGS] --dir /path/to/dir/") << "\n" <<
        bold("Flags") << "\n"
        "  --create             : Create the database if it doesn't already exist.\n"
        "  --writeable          : Allows REST calls to modify the database\n"
        "  --dir DIR            : serve all database files in directory DIR\n"
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
        ;
    }


    int run() override {
        // Read arguments:
        initialize();

        // Start the listener:
        C4RegisterBuiltInWebSocket();
        C4Error err;
        _listener = c4listener_start(&_listenerConfig, &err);
        if (!_listener)
            fail("starting REST listener", err);

        alloc_slice dbName;
        if (_db) {
            // If serving a single database, share it:
            alloc_slice dbPath(c4db_getPath(_db));
            dbName = alloc_slice(c4db_URINameFromPath(dbPath));
            bool started = c4listener_shareDB(_listener, dbName, _db, &err);
            if (!started) {
                cerr << "Got error when starting listener: " << err.domain << " / " << err.code << endl;
                fail("Failed to start listener...");
            }
        }

        // Announce the URL:
        c4log(sRESTLog, kC4LogInfo, "Now listening at %s://localhost:%u/%s",
              (_listenerConfig.tlsConfig ? "https" : "http"),
              _listenerConfig.port,
              (_db ? string(dbName).c_str() : ""));

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

#ifdef COUCHBASE_ENTERPRISE
        string certFile, keyFile, clientCertFile;
#endif

        // Read args:
        processFlags({
            {"--create",        [&]{_dbFlags |= kC4DB_Create; _dbFlags &= ~kC4DB_ReadOnly;}},
            {"--writeable",     [&]{_dbFlags &= ~kC4DB_ReadOnly;}},
            {"--encrypted",     [&]{_dbNeedsPassword = true;}},
            {"--version",       [&]{displayVersion();}},
            {"--verbose",       [&]{verboseFlag();}},
            {"-v",              [&]{verboseFlag();}},
            {"--dir",           [&]{_listenerDirectory = nextArg("directory");}},
            {"--port",          [&]{_listenerConfig.port = (uint16_t)stoul(nextArg("port"));}},
#ifdef COUCHBASE_ENTERPRISE
            {"--cert",          [&]{certFile = nextArg("certificate path");}},
            {"--key",           [&]{keyFile = nextArg("key path");}},
            {"--client-cert",   [&]{clientCertFile = nextArg("certificate path");}},
#endif
        });

        // Set up logging levels:
        sVerboseLog = _verbose;
        c4log_writeToCallback(kC4LogInfo, [](C4LogDomain domain, C4LogLevel level, const char* msg, va_list) {
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
        }, true);

#ifdef COUCHBASE_ENTERPRISE
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
#endif

        if (_listenerDirectory.empty()) {
            openDatabase(nextArg("database path"), false);
        } else {
            _listenerConfig.directory = slice(_listenerDirectory);
#if 0
            if (!(_dbFlags & kC4DB_ReadOnly))
                _listenerConfig.allowCreateDBs = _listenerConfig.allowDeleteDBs = true;
#endif
        }

        endOfArgs();
    }

private:
#ifdef COUCHBASE_ENTERPRISE
    string certName(C4Cert* cert) {
        alloc_slice name(_verbose ? c4cert_summary(cert) : c4cert_subjectName(cert));
        string str(name);
        if (_verbose) str.insert(0, "\n");
        return str;
    }
#endif

    string              _listenerDirectory;
    C4ListenerConfig    _listenerConfig {};  // all false/0
    c4::ref<C4Listener> _listener;
#ifdef COUCHBASE_ENTERPRISE
    TLSConfig           _tlsConfig {};
#endif
};


int main(int argc, const char* argv[]) {
    LiteServTool tool;
    return tool.main(argc, argv);
}


