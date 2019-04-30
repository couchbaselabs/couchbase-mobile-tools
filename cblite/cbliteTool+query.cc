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
    C4QueryLanguage language = kC4JSONQuery;
    if (_currentCommand != "query")         // i.e. it's "select"
        language = kC4N1QLQuery;

    // Read params:
    processFlags(kQueryFlags);
    if (_showHelp) {
        if (language == kC4JSONQuery)
            queryUsage();
        else
            selectUsage();
        return;
    }
    openDatabaseFromNextArg();
    string queryStr = restOfInput("query string");

    // Possibly translate query from N1QL to JSON:
    unsigned queryStartPos = 9;
    if (language == kC4N1QLQuery) {
        queryStr = _currentCommand + " " + queryStr;
    } else if (queryStr[0] != '{' && queryStr[0] != '[') {
        language = kC4N1QLQuery;
        queryStartPos += _currentCommand.size() + 1;
    }

    // Compile query:
    C4Error error;
    size_t errorPos;
    c4::ref<C4Query> query = compileQuery(language, queryStr, &errorPos, &error);
    if (!query) {
        if (error.domain == LiteCoreDomain && error.code == kC4ErrorInvalidQuery) {
            if (_interactive) {
                errorPos += queryStartPos;
            } else {
                cerr << queryStr << "\n";
            }
            cerr << string(errorPos, ' ') << "^\n";
            alloc_slice errorMessage = c4error_getMessage(error);
            fail(string(errorMessage));
        } else {
            fail("compiling query", error);
        }
    }

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


C4Query* CBLiteTool::compileQuery(C4QueryLanguage language, string queryStr,
                                  size_t *outErrorPos, C4Error *outError)
{
    if (language == kC4JSONQuery) {
        // Convert JSON5 to JSON and detect JSON syntax errors:
        FLError flErr;
        FLStringResult outErrMsg;
        alloc_slice queryJSONBuf = FLJSON5_ToJSON(slice(queryStr), &outErrMsg, outErrorPos, &flErr);
        if (!queryJSONBuf) {
            alloc_slice message(move(outErrMsg));
            string messageStr = format("parsing JSON: %.*s", SPLAT(message));
            if (flErr == kFLJSONError)
                *outError = c4error_make(LiteCoreDomain, kC4ErrorInvalidQuery, slice(messageStr));
            else
                *outError = c4error_make(FleeceDomain, flErr, slice(messageStr));
            return nullptr;
        }

        // Trim whitespace from either end:
        slice queryJSON = queryJSONBuf;
        while (isspace(queryJSON[0]))
            queryJSON.moveStart(1);
        while (isspace(queryJSON[queryJSON.size-1]))
            queryJSON.setSize(queryJSON.size-1);

        // Insert OFFSET/LIMIT:
        stringstream json;
        if (queryJSON[0] == '[')
            json << "{\"WHERE\": " << queryJSON;
        else
            json << slice(queryJSON.buf, queryJSON.size - 1);
        if (_offset > 0 || _limit >= 0)
            json << ", \"OFFSET\": [\"$offset\"], \"LIMIT\":  [\"$limit\"]";
        json << "}";
        queryStr = json.str();
    }

    int pos;
    auto query = c4query_new2(_db, language, slice(queryStr), &pos, outError);
    if (!query && outErrorPos)
        *outErrorPos = pos;
    return query;

}
