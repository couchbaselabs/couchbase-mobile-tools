//
// RemoteQueryCommand.cc
//
// Copyright (c) 2022 Couchbase, Inc All rights reserved.
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
#include "RemoteQueryResultWriter.hh"
#include "c4ConnectedClient.hh"
#include "fleece/Mutable.hh"
#include "StringUtil.hh"

using namespace std;
using namespace litecore;
using namespace fleece;

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif


class RemoteQueryCommand : public CBRemoteCommand {
public:
    RemoteQueryCommand(CBRemoteTool &parent, bool n1qlMode)
    :CBRemoteCommand(parent)
    ,_n1qlMode(n1qlMode)
    { }


    void usage() override {
        if (_n1qlMode) {
            writeUsageCommand("select", true, "N1QL_QUERY");
            cerr << "  Runs a N1QL query on the server.\n";
        } else {
            writeUsageCommand("query", true, "QUERY_NAME [PARAM=VALUE ...]");
            cerr << "  Runs a server-side query, given the name assigned to it on the server.\n";
        }
        cerr << "    --raw :      Output JSON (instead of a table)\n"
                "    --json5 :    Omit quotes around alphanmeric keys in JSON output\n"
                "    --offset N : Skip first N rows\n"
                "    --limit N :  Stop after N rows\n";
        if (_n1qlMode) {
            cerr << "  " << it("N1QL_QUERY") << " : A N1QL / SQL++ query string (no quotes needed)\n";
        } else {
            cerr << "  " << it("QUERY_NAME") << " : Name of the query as registered on the server\n"
                    "  " << it("PARAM") << " : Name of a parameter registered for this query\n"
                    "  " << it("VALUE") << " : Value of a parameter; put quotes around it if you use JSON\n";
        }
    }


    void runSubcommand() override {
        // Read params:
        bool unwrap = false;
        unsigned maxWidth = terminalWidth();
        processFlags({
            {"--limit",  [&]{limitFlag();}},
            {"--offset", [&]{offsetFlag();}},
            {"--raw",    [&]{rawFlag();}},
            {"--json5",  [&]{json5Flag();}},
            {"--unwrap", [&]{unwrap = true;}},
            {"--width",  [&]{maxWidth = unsigned(std::stoul(nextArg("max table width")));}},
        });
        openDatabaseFromNextArg();
        string queryName;

        MutableDict params;
        if (_n1qlMode) {
            queryName = "SELECT " + restOfInput("query string");
        } else {
            queryName = nextArg("query name");
            params = MutableDict::newDict();
            while (hasArgs()) {
                // Add a query parameter from an argument of the form `name=value`:
                string param = nextArg("query parameter");
                auto eq = param.find('=');
                if (eq == string::npos)
                    fail("parameter '" + param + "' needs a value; use '=' to separate name and value");
                string value = param.substr(eq + 1);
                param = param.substr(0, eq);

                // Add parameter as a number if it's numeric, else as a string:
                char *end;
                double n = strtod(value.c_str(), &end);
                if (n != 0 && *end == '\0')
                    params[param] = n;
                else
                    params[param] = value;
            }
        }
        endOfArgs();

        unique_ptr<RemoteQueryResultWriter> writer;
        C4Error error {};
        mutex mut;
        condition_variable cond;
        unique_lock lock(mut);

        // select * from _ where meta().id like "airport%"

        // Run the query!
        _db->query(queryName, params, true, [&](slice json, Dict, const C4Error *errp) {
            unique_lock lock(mut);
            if (json) {
                // row:
                if (_offset > 0)
                    --_offset;
                else if (_limit != 0) {
                    if (!writer) {
                        if (_prettyPrint)
                            writer = make_unique<RemoteQueryResultTableWriter>(*this);
                        else
                            writer = make_unique<RemoteQueryResultJSONWriter>(*this);
                        writer->setUnwrap(unwrap);
                        if (maxWidth > 0)
                            writer->setMaxWidth(maxWidth);
                    }
                    writer->addRow(json);
                    if (_limit > 0)
                        --_limit;
                }
            } else {
                // finished:
                if (errp) error = *errp;
                cond.notify_one();
            }
        });

        // Wait for all the rows to be received:
        cond.wait(lock);

        if (error)
            fail("", error);
        else if (writer)
            writer->end();
        else
            cout << "(No rows in result)\n";
    }

private:
    bool            _n1qlMode;
};


CBRemoteCommand* newQueryCommand(CBRemoteTool &parent) {
    return new RemoteQueryCommand(parent, false);
}


CBRemoteCommand* newSelectCommand(CBRemoteTool &parent) {
    return new RemoteQueryCommand(parent, true);
}
