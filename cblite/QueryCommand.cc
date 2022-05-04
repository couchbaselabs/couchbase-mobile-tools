//
// QueryCommand.cc
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
#include "QueryResultWriter.hh"
#include "fleece/FLExpert.h"
#include "StringUtil.hh"

using namespace std;
using namespace litecore;
using namespace fleece;

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif


class QueryCommand : public CBLiteCommand {
public:
    QueryCommand(CBLiteTool &parent, C4QueryLanguage language)
    :CBLiteCommand(parent)
    ,_language(language)
    { }


    void usage() override {
        if (_language == kC4JSONQuery) {
            writeUsageCommand("query", true, "QUERY");
            cerr <<
            "  Runs a query against the database, in JSON or N1QL format.\n"
            "    --raw :      Output JSON (instead of a table)\n"
            "    --json5 :    Omit quotes around alphanmeric keys in JSON output\n"
            "    --offset N : Skip first N rows\n"
            "    --limit N :  Stop after N rows\n"
            "    --explain :  Show SQLite query and explain query plan\n"
            "  " << it("QUERYSTRING") << " : LiteCore JSON or N1QL query expression\n";
        } else {
            writeUsageCommand("select", true, "N1QLSTRING");
            cerr <<
            "  Runs a N1QL query against the database.\n"
            "    --raw :      Output JSON (instead of a table)\n"
            "    --json5 :    Omit quotes around alphanmeric keys in JSON output\n"
            "    --explain :  Show translated SQLite query and explain query plan\n"
            "  " << it("N1QLSTRING") << " : N1QL query, minus the 'SELECT'\n";        }
        if (interactive())
            cerr << "    NOTE: Do not quote the query string, just give it literally.\n";
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--explain",[&]{_explain = true;}},
            {"--limit",  [&]{limitFlag();}},
            {"--offset", [&]{offsetFlag();}},
            {"--raw",    [&]{rawFlag();}},
            {"--json5",  [&]{json5Flag();}},
        });
        openDatabaseFromNextArg();
        string queryStr = restOfInput("query string");

        // Possibly translate query from N1QL to JSON:
        unsigned queryStartPos = 9;
        if (_language == kC4N1QLQuery) {
            queryStr = "SELECT " + queryStr;
        } else if (queryStr[0] != '{' && queryStr[0] != '[') {
            _language = kC4N1QLQuery;
            queryStartPos += 6;  // length of "query "
        }

        // Compile query:
        C4Error error;
        size_t errorPos;
        c4::ref<C4Query> query = compileQuery(_language, queryStr, &errorPos, &error);
        if (!query) {
            if (error.domain == LiteCoreDomain && error.code == kC4ErrorInvalidQuery) {
                if (interactive()) {
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

            unsigned nCols = c4query_columnCount(query);
            vector<string> colTitles;
            for (unsigned col = 0; col < nCols; ++col)
                colTitles.emplace_back(slice(c4query_columnTitle(query, col)));

            unique_ptr<QueryResultWriter> writer;
            if (_prettyPrint)
                writer = make_unique<QueryResultTableWriter>(*this, colTitles);
            else
                writer = make_unique<QueryResultJSONWriter>(*this, colTitles);


            while (c4queryenum_next(e, &error))
                writer->addRow((Array::iterator&)e->columns);
            if (error.code)
                fail("running query", error);
            writer->end();
        }
    }


    C4Query* compileQuery(C4QueryLanguage language, string queryStr,
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


    static bool prefixMatch(const string &prefix, const char *word) {
        for (auto pc : prefix) {
            if (tolower(pc) != tolower(*word++))
                return false;
        }
        return true;
    }


    void addLineCompletions(ArgumentTokenizer &tokenizer,
                            function<void(const string&)> addCompletion) override
    {
        static constexpr const char* kVocabulary[] = {
            // N1QL keywords -- adapted from list of identifiers in n1ql.leg in LiteCore.
            // Slightly reordered so the more common words come first.
            "ALL ",
            "AND ",
            "ANY ",
            "ANY AND EVERY ",
            "AS ",
            "ASC ",
            "BETWEEN ",
            "BY ",
            "CASE ",
            "COLLATE ",
            "CROSS JOIN ",
            "DISTINCT ",
            "DESC ",
            "ELSE ",
            "END ",
            "EVERY ",
            "EXISTS ",
//            "FROM ",
            "FALSE ",
            "GROUP ",
            "HAVING ",
            "IN ",
            "INNER ",
            "IS ",
            "IS NOT ",
            "JOIN ",
            "LEFT JOIN ",
            "LEFT OUTER JOIN ",
            "LIKE ",
            "LIMIT ",
            "MISSING ",
            "MATCH ",
            "META.id",
            "META.sequence",
            "META.deleted",
            "META.expiration",
            "NOT ",
            "NOT IN ",
            "NOT NULL ",
            "NULL ",
            "OFFSET ",
            "ON ",
            "OR ",
            "ORDER ",
            "OUTER JOIN ",
            "SELECT ",
            "SOME ",
            "SATISFIES ",
            "THEN ",
            "TRUE ",
            "WHERE ",
            "WHEN ",

            // Functions -- adapted from list in QueryParserTables.hh
            "array_avg(",
            "array_contains(",
            "array_count(",
            "array_ifnull(",
            "array_length(",
            "array_max(",
            "array_min(",
            "array_of(",
            "array_sum(",

            "greatest(",
            "least(",

            "ifmissing(",
            "ifnull(",
            "ifmissingornull(",
            "missingif(",
            "nullif(",

            "millis_to_str(",
            "millis_to_utc(",
            "str_to_millis(",
            "str_to_utc(",

            "abs(",
            "acos(",
            "asin(",
            "atan(",
            "atan2(",
            "ceil(",
            "cos(",
            "degrees(",
            "e(",
            "exp(",
            "floor(",
            "ln(",
            "log(",
            "pi(",
            "power(",
            "radians(",
            "round(",
            "sign(",
            "sin(",
            "sqrt(",
            "tan(",
            "trunc(",

            "regexp_contains(",
            "regexp_like(",
            "regexp_position(",
            "regexp_replace(",
            "fl_like(",

            "concat(",
            "contains(",
            "length(",
            "lower(",
            "ltrim(",
            "rtrim(",
            "trim(",
            "upper(",

            "isarray(",
            "isatom(",
            "isboolean(",
            "isnumber(",
            "isobject(",
            "isstring(",
            "type(",
            "toarray(",
            "toatom(",
            "toboolean(",
            "tonumber(",
            "toobject(",
            "tostring(",

            "rank(",

            "avg(",
            "count(",
            "max(",
            "min(",
            "sum(",

#ifdef COUCHBASE_ENTERPRISE
            "prediction(",
            "euclidean_distance(",
            "cosine_distance(",
#endif
        };

        if (_language != kC4N1QLQuery)
            return;

        // Extract the final word from the command line. This is N1QL syntax, not regular arguments,
        // so treat any non-alphanumeric character as a delimiter.
        string cmd = tokenizer.restOfInput();
        int max = int(cmd.size()) - 1;
        int p;
        for (p = max; p >= 0; --p)
            if (!isalnum(cmd[p]))
                break;
        if (p == max)
            return;

        string word = cmd.substr(p + 1);
        cmd.resize(p + 1);

        for (auto &ident : kVocabulary) {
            if (prefixMatch(word, ident))
                addCompletion(cmd + ident);
        }
    }

private:
    C4QueryLanguage         _language;
    bool                    _explain {false};
};


CBLiteCommand* newQueryCommand(CBLiteTool &parent) {
    return new QueryCommand(parent, kC4JSONQuery);
}

CBLiteCommand* newSelectCommand(CBLiteTool &parent) {
    return new QueryCommand(parent, kC4N1QLQuery);
}
