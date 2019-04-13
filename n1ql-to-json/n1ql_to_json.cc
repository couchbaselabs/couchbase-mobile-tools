//
//  n1ql_2_json.cc
//  N1QL to JSON
//
//  Created by Jens Alfke on 4/4/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#include "n1ql_to_json.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include "n1ql_parser.h"

using namespace std;
using namespace fleece;


char* c4query_translateN1QL(C4String n1ql,
                            C4TranslateN1QLFlags flags,
                            char** outErrorMessage,
                            unsigned* outErrorPosition,
                            unsigned* outErrorLine) C4API
{
    int errPos;
    MutableDict result = n1ql_parse(string((char*)n1ql.buf, n1ql.size), &errPos);
    if (result) {
        alloc_slice json = result.toJSON( (flags & kN1QLToJSON5) != 0,
                                          (flags & kN1QLToCanonicalJSON) != 0);
        return strdup(string(json).c_str());
    } else {
        if (outErrorMessage)
            *outErrorMessage = strdup("Syntax error");
        if (outErrorPosition)
            *outErrorPosition = errPos;
        if (outErrorLine)
            *outErrorLine = 0;
        return nullptr;
    }
}
