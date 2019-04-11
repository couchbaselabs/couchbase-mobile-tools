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
        char* error;
        unsigned errorPos, errorLine;
        char *json = c4query_translateN1QL(c4str(n1ql), &error, &errorPos, &errorLine);
        INFO(error << " at " << errorLine << ":" << errorPos);
        string result = (json ? json : "");
        std::cerr << n1ql << "  -->  " << result << "\n";
        CHECK(json);
        free(json);
        return result;
    }
};

TEST_CASE_METHOD(ParserTestFixture, "N1QL to JSON") {
    CHECK(translate("SELECT foo") == "{\"WHAT\":[[\".foo\"]]}");
}
