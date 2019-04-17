//
//  n1ql_to_json.cc
//
// Copyright (c) 2019 Couchbase, Inc. All rights reserved.
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

#include "n1ql_to_json.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include "n1ql_parser.hh"

using namespace std;
using namespace fleece;


char* c4query_translateN1QL(C4String n1ql,
                            C4TranslateN1QLFlags flags,
                            char** outErrorMessage,
                            unsigned* outErrorPosition,
                            unsigned* outErrorLine) C4API
{
    int errPos;
    MutableDict result = litecore::n1ql::parse(string((char*)n1ql.buf, n1ql.size), &errPos);
    if (result) {
        alloc_slice json = result.toJSON( (flags & kN1QLToJSON5) != 0,
                                          (flags & kN1QLToCanonicalJSON) != 0);
        return strdup(string(json).c_str());
    } else {
        if (outErrorMessage)
            *outErrorMessage = strdup("N1QL syntax error");
        if (outErrorPosition)
            *outErrorPosition = errPos;
        if (outErrorLine)
            *outErrorLine = 0;
        return nullptr;
    }
}
