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

TEST_CASE_METHOD(ParserTestFixture, "N1QL literals", "[N1QL]") {
    CHECK(translate("SELECT FALSE") == "{WHAT:[false]}");
    CHECK(translate("SELECT TRUE") == "{WHAT:[true]}");
    CHECK(translate("SELECT NULL") == "{WHAT:[null]}");
    CHECK(translate("SELECT MISSING") == "{WHAT:[['MISSING']]}");

    CHECK(translate("SELECT 0") == "{WHAT:[0]}");
    CHECK(translate("SELECT 17") == "{WHAT:[17]}");
    CHECK(translate("SELECT -17") == "{WHAT:[-17]}");
    CHECK(translate("SELECT 17.25") == "{WHAT:[17.25]}");
    CHECK(translate("SELECT -17.25") == "{WHAT:[-17.25]}");
    CHECK(translate("SELECT 17.25e2") == "{WHAT:[1725]}");
    CHECK(translate("SELECT 17.25E+02") == "{WHAT:[1725]}");
    CHECK(translate("SELECT 17.25e02") == "{WHAT:[1725]}");
    CHECK(translate("SELECT 1625e-02") == "{WHAT:[16.25]}");
    CHECK(translate("SELECT .25") == "{WHAT:[0.25]}");
    
    CHECK(translate("SELECT 'hi'") == "{WHAT:['hi']}");
    CHECK(translate("SELECT 'foo bar'") == "{WHAT:['foo bar']}");
    CHECK(translate("SELECT 'foo ''or'' bar'") == "{WHAT:['foo 'or' bar']}");
}

TEST_CASE_METHOD(ParserTestFixture, "N1QL properties", "[N1QL]") {
    CHECK(translate("select foo") == "{WHAT:[['.foo']]}");
    CHECK(translate("select foo.bar") == "{WHAT:[['.foo.bar']]}");
    CHECK(translate("select foo. bar . baz") == "{WHAT:[['.foo.bar.baz']]}");
    
    CHECK(translate("select \"foo bar\"") == "{WHAT:[['.foo bar']]}");
    CHECK(translate("select \"foo \"\"bar\"\"baz\"") == "{WHAT:[['.foo \\'bar\\'baz']]}");

    CHECK(translate("select \"mr.grieves\".\"hey\"") == "{WHAT:[['.mr\\\\.grieves.hey']]}");
    CHECK(translate("select \"$type\"") == "{WHAT:[['.\\\\$type']]}");

    CHECK(translate("select meta.id") == "{WHAT:[['._id']]}");
    CHECK(translate("select meta.sequence") == "{WHAT:[['._sequence']]}");
    CHECK(translate("select meta.deleted") == "{WHAT:[['._deleted']]}");
    CHECK(translate("select db.meta.id") == "{WHAT:[['.db._id']]}");
    CHECK(translate("select meta.bogus") == "");    // only specific meta properties allowed
    CHECK(translate("select db.meta.bogus") == "");

    CHECK(translate("SELECT *") == "{WHAT:[['.']]}");
    CHECK(translate("SELECT db.*") == "{WHAT:[['.db.']]}");

    CHECK(translate("select $var") == "{WHAT:[['$var']]}");
}

TEST_CASE_METHOD(ParserTestFixture, "N1QL expressions", "[N1QL]") {
    CHECK(translate("SELECT -x") == "{WHAT:[['-',['.x']]]}");
    CHECK(translate("SELECT NOT x") == "{WHAT:[['NOT',['.x']]]}");
    
    CHECK(translate("SELECT 17+0") == "{WHAT:[['+',17,0]]}");
    CHECK(translate("SELECT 17 + 0") == "{WHAT:[['+',17,0]]}");
    CHECK(translate("SELECT 17 > 0") == "{WHAT:[['>',17,0]]}");
    CHECK(translate("SELECT 17='hi'") == "{WHAT:[['=',17,'hi']]}");
    CHECK(translate("SELECT 17 = 'hi'") == "{WHAT:[['=',17,'hi']]}");
    CHECK(translate("SELECT 17 IN primes") == "{WHAT:[['IN',17,['.primes']]]}");
    CHECK(translate("SELECT 17 NOT IN primes") == "{WHAT:[['NOT IN',17,['.primes']]]}");
    CHECK(translate("SELECT 17 NOT  IN primes") == "{WHAT:[['NOT IN',17,['.primes']]]}");

    CHECK(translate("SELECT 6 IS 9") == "{WHAT:[['IS',6,9]]}");
    CHECK(translate("SELECT 6 IS NOT 9") == "{WHAT:[['IS NOT',6,9]]}");
    CHECK(translate("SELECT 6 NOT NULL") == "{WHAT:[['IS NOT',6,null]]}");

    CHECK(translate("SELECT 2 BETWEEN 1 AND 4") == "{WHAT:[['BETWEEN',2,1,4]]}");
    CHECK(translate("SELECT 2+3 BETWEEN 1+1 AND 4+4") == "{WHAT:[['BETWEEN',['+',2,3],['+',1,1],['+',4,4]]]}");

    // Check for left-associativity and correct operator precedence:
    CHECK(translate("SELECT 3 + 4 + 5 + 6") == "{WHAT:[['+',['+',['+',3,4],5],6]]}");
    CHECK(translate("SELECT 3 - 4 - 5 - 6") == "{WHAT:[['-',['-',['-',3,4],5],6]]}");
    CHECK(translate("SELECT 3 + 4 * 5 - 6") == "{WHAT:[['-',['+',3,['*',4,5]],6]]}");

    CHECK(translate("SELECT (3 + 4) * (5 - 6)") == "{WHAT:[['*',['+',3,4],['-',5,6]]]}");

    CHECK(translate("SELECT type='airline' and callsign not null") == "{WHAT:[['and',['=',['.type'],'airline'],['IS NOT',['.callsign'],null]]]}");

    //CHECK(translate("SELECT CASE x WHEN 1 THEN 'one' END") == "{WHAT:['CASE',['.x'],['WHEN',1,'one']]}"); // TODO

    CHECK(translate("SELECT squee()") == "{WHAT:[['squee()']]}");
    CHECK(translate("SELECT squee(1)") == "{WHAT:[['squee()',1]]}");
    CHECK(translate("SELECT squee(1, 2)") == "{WHAT:[['squee()',1,2]]}");
    CHECK(translate("SELECT squee(1, flup(2))") == "{WHAT:[['squee()',1,['flup()',2]]]}");

    CHECK(translate("SELECT count(*)") == "{WHAT:[['count()',['.']]]}");
    CHECK(translate("SELECT count(db.*)") == "{WHAT:[['count()',['.db.']]]}");
}

TEST_CASE_METHOD(ParserTestFixture, "N1QL SELECT", "[N1QL]") {
    CHECK(translate("SELECT foo bar") == "");
    CHECK(translate("SELECT foo, bar") == "{WHAT:[['.foo'],['.bar']]}");
    CHECK(translate("SELECT foo as A, bar as B") == "{WHAT:[['AS',['.foo'],'A'],['AS',['.bar'],'B']]}");

    CHECK(translate("SELECT foo WHERE 10") == "{WHAT:[['.foo']],WHERE:10}");
    CHECK(translate("SELECT WHERE 10") == "");
    CHECK(translate("SELECT foo WHERE foo = 'hi'") == "{WHAT:[['.foo']],WHERE:['=',['.foo'],'hi']}");

    CHECK(translate("SELECT foo GROUP BY bar") == "{GROUP_BY:[['.bar']],WHAT:[['.foo']]}");
    CHECK(translate("SELECT foo GROUP BY bar, baz") == "{GROUP_BY:[['.bar'],['.baz']],WHAT:[['.foo']]}");
    CHECK(translate("SELECT foo GROUP BY bar, baz HAVING hi") == "{GROUP_BY:[['.bar'],['.baz']],HAVING:['.hi'],WHAT:[['.foo']]}");

    CHECK(translate("SELECT foo ORDER BY bar") == "{ORDER_BY:[['.bar']],WHAT:[['.foo']]}");
    CHECK(translate("SELECT foo ORDER BY bar DESC") == "{ORDER_BY:[['DESC',['.bar']]],WHAT:[['.foo']]}");

    CHECK(translate("SELECT foo LIMIT 10") == "{LIMIT:10,WHAT:[['.foo']]}");
    CHECK(translate("SELECT foo LIMIT 10 OFFSET 20") == "{LIMIT:10,OFFSET:20,WHAT:[['.foo']]}");
}

TEST_CASE_METHOD(ParserTestFixture, "N1QL JOIN", "[N1QL]") {
    CHECK(translate("SELECT 0 FROM db") == "{FROM:[{AS:'db'}],WHAT:[0]}");
    CHECK(translate("SELECT file.name FROM db AS file") == "{FROM:[{AS:'file'}],WHAT:[['.file.name']]}");
    CHECK(translate("SELECT db.name FROM db JOIN db AS other ON other.key = db.key")
          == "{FROM:[{AS:'db'},{AS:'other',JOIN:'LEFT',ON:['=',['.other.key'],['.db.key']]}],WHAT:[['.db.name']]}");
    CHECK(translate("SELECT db.name FROM db JOIN db AS other ON other.key = db.key CROSS JOIN x ON 17")
          == "{FROM:[{AS:'db'},{AS:'other',JOIN:'LEFT',ON:['=',['.other.key'],['.db.key']]},{AS:'other',JOIN:'CROSS',ON:17}],WHAT:[['.db.name']]}");
}