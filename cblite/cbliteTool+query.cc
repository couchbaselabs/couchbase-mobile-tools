//
// cbliteTool+Query.cc
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
#include "n1ql_to_json.h"
#include "StringUtil.hh"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif


const Tool::FlagSpec CBLiteTool::kQueryFlags[] = {
    {"--explain",(FlagHandler)&CBLiteTool::explainFlag},
    {"--help",   (FlagHandler)&CBLiteTool::helpFlag},
    {"--limit",  (FlagHandler)&CBLiteTool::limitFlag},
    {"--offset", (FlagHandler)&CBLiteTool::offsetFlag},
    {"--raw",    (FlagHandler)&CBLiteTool::rawFlag},
    {"--json5",  (FlagHandler)&CBLiteTool::json5Flag},
    {nullptr, nullptr}
};

void CBLiteTool::queryUsage() {
    writeUsageCommand("query", true, "QUERY");
    cerr <<
    "  Runs a query against the database, in JSON or N1QL format.\n"
    "    --raw :      Output JSON (instead of a table)\n"
    "    --json5 :    Omit quotes around alphanmeric keys in JSON output\n"
    "    --offset N : Skip first N rows\n"
    "    --limit N :  Stop after N rows\n"
    "    --explain :  Show SQLite query and explain query plan\n"
    "  " << it("QUERYSTRING") << " : LiteCore JSON or N1QL query expression\n";
    if (_interactive)
        cerr << "    NOTE: Do not quote the query string, just give it literally.\n";
}


void CBLiteTool::selectUsage() {
    writeUsageCommand("select", true, "N1QLSTRING");
    cerr <<
    "  Runs a N1QL query against the database.\n"
    "    --raw :      Output JSON (instead of a table)\n"
    "    --json5 :    Omit quotes around alphanmeric keys in JSON output\n"
    "    --explain :  Show translated SQLite query and explain query plan\n"
    "  " << it("N1QLSTRING") << " : N1QL query, minus the 'SELECT'\n";
    if (_interactive)
        cerr << "    NOTE: Do not quote the query string, just give it literally.\n";
}


void CBLiteTool::queryDatabase() {
    bool isN1QL = (_currentCommand != "query");         // i.e. it's "select"
    // Read params:
    processFlags(kQueryFlags);
    if (_showHelp) {
        isN1QL ? selectUsage() : queryUsage();
        return;
    }
    openDatabaseFromNextArg();
    string queryStr = restOfInput("query string");

    // Possibly translate query from N1QL to JSON:
    unsigned queryStartPos = 9;
    if (isN1QL) {
        queryStr = _currentCommand + " " + queryStr;
    } else if (queryStr[0] != '{' && queryStr[0] != '[') {
        isN1QL = true;
        queryStartPos += _currentCommand.size() + 1;
    }

    if (isN1QL) {
        char *errorMessage;
        unsigned errorPos;
        char *jsonQuery = c4query_translateN1QL(slice(queryStr), 0,
                                                &errorMessage, &errorPos, nullptr);
        if (!jsonQuery) {
            string failure = "parsing N1QL";
            if (_interactive) {
                errorPos += queryStartPos;
            } else {
                cerr << queryStr << "\n";
            }
            cerr << string(errorPos, ' ') << "^"
                 << (errorPos < 70 ? "---" : "^\n") << errorMessage << "\n";
            free(errorMessage);
            fail(failure);
        }
        queryStr = jsonQuery;
        free(jsonQuery);
//        if (!_explain)
//            cout << ansiDim() << "As JSON: " << queryStr << ansiReset() << "\n";
    }
    alloc_slice queryJSON = convertQuery(queryStr);

    // Compile JSON query:
    C4Error error;
    c4::ref<C4Query> query = c4query_new(_db, queryJSON, &error);
    if (!query)
        fail("compiling query", error);

    if (_explain) {
        // Explain query plan:
        alloc_slice explanation = c4query_explain(query);
        cout << explanation;

    } else {
        // Run query:
        // Set parameters:
        alloc_slice params;
        if (_offset > 0 || _limit >= 0) {
            JSONEncoder enc;
            enc.beginDict();
            enc.writeKey("offset"_sl);
            enc.writeInt(_offset);
            enc.writeKey("limit"_sl);
            enc.writeInt(_limit);
            enc.endDict();
            params = enc.finish();
        }
        c4::ref<C4QueryEnumerator> e = c4query_run(query, nullptr, params, &error);
        if (!e)
            fail("starting query", error);

        if (_prettyPrint)
            displayQueryAsTable(query, e);
        else
            displayQueryAsJSON(query, e);
    }
}


void CBLiteTool::displayQueryAsJSON(C4Query *query, C4QueryEnumerator *e) {
    unsigned nCols = c4query_columnCount(query);
    vector<string> colTitle(nCols);

    for (unsigned col = 0; col < nCols; ++col) {
        auto title = string(slice(c4query_columnTitle(query, col)));
        if (!_json5 || !canBeUnquotedJSON5Key(title))
            title = "\"" + title + "\"";
        colTitle[col] = title;
    }

    uint64_t nRows = 0;
    C4Error error;
    cout << "[";
    while (c4queryenum_next(e, &error)) {
        // Write a result row:
        if (nRows++)
            cout << ",\n ";
        cout << "{";
        int col = 0, n = 0;
        for (Array::iterator i(e->columns); i; ++i, ++col) {
            if (!(e->missingColumns & (1<<col))) {
                if (n++)
                    cout << ", ";
                cout << colTitle[col] << ": ";
                rawPrint(i.value(), nullslice);
            }
        }
        cout << "}";
    }
    if (error.code)
        fail("running query", error);
    cout << "]\n";
}


void CBLiteTool::displayQueryAsTable(C4Query *query, C4QueryEnumerator *e) {
    unsigned nCols = c4query_columnCount(query);
    uint64_t nRows;
    vector<size_t> widths(nCols);
    unsigned col;
    C4Error error;

    // Compute the column widths:
    for (col = 0; col < nCols; ++col) {
        auto title = c4query_columnTitle(query, col);
        widths[col] = title.size;  // not UTF-8-aware...
    }
    nRows = 0;
    while (c4queryenum_next(e, &error)) {
        ++nRows;
        col = 0;
        for (Array::iterator i(e->columns); i; ++i) {
            if (!(e->missingColumns & (1<<col))) {
                size_t width;
                if (i.value().type() == kFLString)
                    width = i.value().asString().size;
                else
                    width = i.value().toJSON(_json5, true).size;
                widths[col] = max(widths[col], width);
            }
            ++col;
        }
    }
    if (error.code || !c4queryenum_restart(e, &error))
        fail("running query", error);
    if (nRows == 0) {
        cout << "(No results)\n";
        return;
    }

    // Subroutine that writes a column:
    auto writeCol = [&](slice s, int align) {
        string pad(widths[col] - s.size, ' ');
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
        cout << ansiBold();
        for (col = 0; col < nCols; ++col)
            writeCol(c4query_columnTitle(query, col), 1);
        cout << "\n";
        for (col = 0; col < nCols; ++col)
            cout << string(widths[col], '_') << ' ';
        cout << ansiReset() << "\n";
    }

    // Write the rows:
    nRows = 0;
    while (c4queryenum_next(e, &error)) {
        // Write a result row:
        ++nRows;
        col = 0;
        for (Array::iterator i(e->columns); i; ++i) {
            alloc_slice json;
            if (!(e->missingColumns & (1<<col))) {
                auto type = i.value().type();
                if (type == kFLString)
                    writeCol(i.value().asString(), 1);
                else
                    writeCol(i.value().toJSON(_json5, true),
                             (type == kFLNumber ? -1 : 1));
            }
            ++col;
        }
        cout << "\n";
    }
    if (error.code)
        fail("running query", error);
}


alloc_slice CBLiteTool::convertQuery(slice inputQuery) {
    FLError flErr;
    alloc_slice queryJSONBuf = FLJSON5_ToJSON(slice(inputQuery), &flErr);
    if (!queryJSONBuf)
        fail("Query is neither N1QL nor valid JSON");

    // Trim whitespace from either end:
    slice queryJSON = queryJSONBuf;
    while (isspace(queryJSON[0]))
        queryJSON.moveStart(1);
    while (isspace(queryJSON[queryJSON.size-1]))
        queryJSON.setSize(queryJSON.size-1);

    stringstream json;
    if (queryJSON[0] == '[')
        json << "{\"WHERE\": " << queryJSON;
    else
        json << slice(queryJSON.buf, queryJSON.size - 1);
    if (_offset > 0 || _limit >= 0)
        json << ", \"OFFSET\": [\"$offset\"], \"LIMIT\":  [\"$limit\"]";
    json << "}";
    return alloc_slice(json.str());
}


