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
        processFlags({
            {"--limit",  [&]{limitFlag();}},
            {"--offset", [&]{offsetFlag();}},
            {"--raw",    [&]{rawFlag();}},
            {"--json5",  [&]{json5Flag();}},
        });
        openDatabaseFromNextArg();
        string queryName = nextArg("query name");

        MutableDict params;
        if (_n1qlMode) {
            queryName = "SELECT " + queryName;
        } else {
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

        unique_ptr<Writer> writer;
        C4Error error {};
        mutex mut;
        condition_variable cond;
        unique_lock lock(mut);

        // Run the query!
        _db->query(queryName, params, [&](Array row, const C4Error *errp) {
            unique_lock lock(mut);
            if (!row) { // finished
                if (errp) error = *errp;
                cond.notify_one();
            } else if (writer) {
                writer->addRow(row);
            } else { // first row
                if (_prettyPrint)
                    writer = make_unique<JSONWriter>(*this, row);
                else
                    writer = make_unique<TableWriter>(*this, row);
            }
        });

        // Wait for all the rows to be received:
        cond.wait(lock);

        if (error)
            fail("error from server", error);
        else if (writer)
            writer->end();
        else
            cout << "(No rows in result)\n";
    }


    // Abstract class that writes the result rows of a query.
    class Writer {
    public:
        Writer(RemoteQueryCommand &cmd, Array colNames)
        :_command(cmd)
        ,_columnNames(colNames.mutableCopy(kFLDeepCopyImmutables))
        { }

        virtual ~Writer() = default;
        unsigned rowNumber() const              {return _rowNumber;}
        virtual void addRow(Array row)          {++_rowNumber;}
        virtual void end()                      { }

    protected:
        RemoteQueryCommand& _command;
        MutableArray        _columnNames;
        unsigned            _rowNumber = 0;
    };


    class JSONWriter : public Writer {
    public:
        JSONWriter(RemoteQueryCommand &cmd, Array colNames)
        :Writer(cmd, colNames)
        {
            cout << "[\n";
        }

        void addRow(Array row) override {
            Writer::addRow(row);
            if (_rowNumber > 1)
                cout << ",\n ";
            cout << "{";
            int col = 0;
            for (Array::iterator i(row); i; ++i, ++col) {
                if (col > 0)
                    cout << ", ";
                cout << _columnNames[col].asString() << ": ";
                _command.rawPrint(i.value(), nullslice);
            }
            cout << "}";
        }

        void end() override {
            cout << "]\n";
        }
    };


    class TableWriter : public Writer {
    public:
        TableWriter(RemoteQueryCommand &cmd, Array colNames)
        :Writer(cmd, colNames)
        ,_rows(MutableArray::newArray())
        {
            for (Array::iterator i(_columnNames); i; ++i)
                _columnWidths.push_back(i->asString().size);  // not UTF-8-aware...
        }

        void addRow(Array row) override {
            Writer::addRow(row);
            size_t col = 0;
            for (Array::iterator i(row); i; ++i, ++col) {
                // Compute the column width, in this row:
                size_t width;
                if (i.value().type() == kFLString)
                    width = i.value().asString().size;  // not UTF-8-aware...
                else
                    width = i.value().toJSON(_command._json5, true).size;
                _columnWidths[col] = max(_columnWidths[col], width);
            }
            // Buffer the row, and write it at the end, when the column widths are known.
            _rows.append(row);
        }

        void end() override {
            unsigned nCols = _columnNames.count();
            unsigned col;

            // Subroutine that writes a column:
            auto writeCol = [&](slice s, int align) {
                string pad(_columnWidths[col] - s.size, ' ');
                if (align < 0)
                    cout << pad;
                cout << s;
                if (col < nCols-1) {
                    if (align > 0)
                        cout << pad;
                    cout << ' ';
                }
            };

            // Write the column titles:
            if (nCols > 1) {
                cout << _command.ansiBold();
                for (col = 0; col < nCols; ++col)
                    writeCol(_columnNames[col].asstring(), 1);
                cout << "\n";
                for (col = 0; col < nCols; ++col)
                    cout << string(_columnWidths[col], '_') << ' ';
                cout << _command.ansiReset() << "\n";
            }

            // Write the rows:
            for (Array::iterator irow(_rows); irow; ++irow) {
                // Write a result row:
                col = 0;
                for (Array::iterator i(irow->asArray()); i; ++i) {
                    alloc_slice json;
                    auto type = i.value().type();
                    if (type == kFLString)
                        writeCol(i.value().asString(), 1);
                    else
                        writeCol(i.value().toJSON(_command._json5, true),
                                 (type == kFLNumber ? -1 : 1));
                }
                cout << "\n";
            }
        }

    private:
        vector<size_t>  _columnWidths;
        MutableArray    _rows;
    };

private:
    bool            _n1qlMode;
};


CBRemoteCommand* newQueryCommand(CBRemoteTool &parent) {
    return new RemoteQueryCommand(parent, false);
}


CBRemoteCommand* newSelectCommand(CBRemoteTool &parent) {
    return new RemoteQueryCommand(parent, true);
}
