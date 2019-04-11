//
// N1QLParserTests.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "catch.hpp"
#include "CatchHelper.hh"
#include <iostream>

#include "n1ql_to_json.h"

using namespace std;

class ParserTestFixture {
protected:
    string translate(const char *n1ql) {
        std::cerr << n1ql << "  -->  " ;
        char* error;
        unsigned errorPos, errorLine;
        char *json = c4query_translateN1QL(c4str(n1ql),
                                           kN1QLToJSON5 | kN1QLToCanonicalJSON,
                                           &error, &errorPos, &errorLine);
        string result;
        if (json) {
            result = json;
            size_t pos;
            while (string::npos != (pos = result.find('"')))
                result[pos] = '\'';
            free(json);
            std::cerr << result << "\n";
        } else {
            std::cerr << "!! " << error << " (at " << errorLine << ":" << errorPos << ")\n"
                << string(errorPos, ' ') << "^\n";
        }
        return result;
    }
};

TEST_CASE_METHOD(ParserTestFixture, "N1QL to JSON", "[N1QL]") {
    CHECK(translate("select foo") == "{WHAT:[['.foo']]}");
    CHECK(translate("SELECT 17") == "{WHAT:[17]}");
    CHECK(translate("SELECT -17") == "{WHAT:[-17]}");
//    CHECK(translate("SELECT -17.25") == "{WHAT:[-17.25]}");
    CHECK(translate("SELECT 'hi'") == "{WHAT:['hi']}");
    CHECK(translate("SELECT FALSE") == "{WHAT:[false]}");
    CHECK(translate("SELECT TRUE") == "{WHAT:[true]}");
    CHECK(translate("SELECT 17+0") == "{WHAT:[['+',17,0]]}");
    CHECK(translate("SELECT 17 + 0") == "{WHAT:[['+',17,0]]}");
    CHECK(translate("SELECT 17 > 0") == "{WHAT:[['>',17,0]]}");
    CHECK(translate("SELECT 17='hi'") == "{WHAT:[['=',17,'hi']]}");
    CHECK(translate("SELECT 17 = 'hi'") == "{WHAT:[['=',17,'hi']]}");
    CHECK(translate("SELECT 17 IN primes") == "{WHAT:[['IN',17,['.primes']]]}");
    CHECK(translate("SELECT 17 NOT IN primes") == "{WHAT:[['NOT IN',17,['.primes']]]}");
    CHECK(translate("SELECT 17 NOT  IN primes") == "{WHAT:[['NOT IN',17,['.primes']]]}");

    CHECK(translate("SELECT 3 + 4 + 5 + 6") == "{WHAT:[['+',3,['+',4,['+',5,6]]]]}");
    CHECK(translate("SELECT 3 - 4 - 5 - 6") == "");
    CHECK(translate("SELECT 3 + 4 * 5 - 6") == "{WHAT:[['+',3,['-',['*',4,5],6]]]}");

    CHECK(translate("SELECT foo bar") == "");
    CHECK(translate("SELECT foo, bar") == "{WHAT:[['.foo'],['.bar']]}");
    CHECK(translate("SELECT foo WHERE 10") == "{WHAT:[['.foo']],WHERE:10}");
    CHECK(translate("SELECT WHERE 10") == "");
    CHECK(translate("SELECT foo WHERE foo = 'hi'") == "{WHAT:[['.foo']],WHERE:['=',['.foo'],'hi']}");
}
