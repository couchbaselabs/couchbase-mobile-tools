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
#include "n1ql2json.hh"
#include "StringUtil.hh"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif


const Tool::FlagSpec CBLiteTool::kQueryFlags[] = {
    {"--explain",(FlagHandler)&CBLiteTool::explainFlag},
    {"--help",   (FlagHandler)&CBLiteTool::helpFlag},
    {"--limit",  (FlagHandler)&CBLiteTool::limitFlag},
    {"--offset", (FlagHandler)&CBLiteTool::offsetFlag},
    {nullptr, nullptr}
};

void CBLiteTool::queryUsage() {
    writeUsageCommand("query", true, "QUERYSTRING");
    cerr <<
    "  Runs a query against the database, in JSON or N1QL format.\n"
    "    --offset N : Skip first N rows\n"
    "    --limit N : Stop after N rows\n"
    "    --explain : Show SQL query and explain query plan\n"
    "  " << it("QUERYSTRING") << " : LiteCore JSON (or JSON5) or N1QL query expression\n";
    if (_interactive)
        cerr << "    NOTE: Do not quote the query string, just give it literally.\n";
}


void CBLiteTool::selectUsage() {
    writeUsageCommand("select", true, "N1QLSTRING");
    cerr <<
    "  Runs a N1QL query against the database.\n"
    "    --explain : Show translated SQL query and explain query plan\n"
    "  " << it("N1QLSTRING") << " : N1QL query, minus the 'SELECT'\n";
    if (_interactive)
        cerr << "    NOTE: Do not quote the query string, just give it literally.\n";
}


void CBLiteTool::queryDatabase() {
    bool isN1QL = (_currentCommand != "query");
    // Read params:
    processFlags(kQueryFlags);
    if (_showHelp) {
        isN1QL ? selectUsage() : queryUsage();
        return;
    }
    openDatabaseFromNextArg();
    string queryStr = restOfInput("query string");

    if (isN1QL)
        queryStr = _currentCommand + " " + queryStr;
    else if (strncasecmp(queryStr.c_str(), "SELECT ", 7) == 0)
        isN1QL = true;

    if (isN1QL) {
        string parseError;
        queryStr = litecore::n1ql::N1QL_to_JSON(queryStr, parseError);
        if (queryStr.empty())
            fail("N1QL parse error: " + parseError);
        cout << ansiDim() << "As JSON: " << queryStr << ansiReset() << "\n";
    }
    alloc_slice queryJSON = convertQuery(queryStr);

    // Compile query:
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
        if (_offset > 0)
            cout << "(Skipping first " << _offset << " rows)\n";

        // Write the column titles:
        cout << "[ ";
        unsigned nCols = c4query_columnCount(query);
        unsigned width = 2 + 2 * nCols;
        for (unsigned i = 0; i < nCols; ++i) {
            if (i > 0)
                cout << ", ";
            auto title = c4query_columnTitle(query, i);
            cout << title;
            width += title.size;  // not UTF-8-aware...
        }
        cout << " ]\n" << string(width, '-') << "\n";

        uint64_t nRows = 0;
        while (c4queryenum_next(e, &error)) {
            // Write a result row:
            ++nRows;
            cout << "[";
            int col = 0;
            for (Array::iterator i(e->columns); i; ++i) {
                if (col++)
                    cout << ", ";
                rawPrint(i.value(), nullslice);
            }
            cout << "]\n";
        }
        if (error.code)
            fail("running query", error);
        if (nRows == _limit)
            cout << "(Limit was " << _limit << " rows)\n";
    }
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


