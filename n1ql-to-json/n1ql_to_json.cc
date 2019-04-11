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
#include <sstream>

#include "n1ql_parser.h"

using namespace std;
using namespace fleece;

static stringstream* sInput; //TODO: Use context instead of globals
static unsigned sInputPos;

antlrcpp::Any* sN1QLResult;


int n1ql_input(char *buf, size_t max_size) {
    sInput->get(buf, max_size+1, -1);
    auto n = sInput->gcount();
    sInputPos += n;
    return (int)n;
}

/*
struct _n1ql_aux {
    _n1ql_aux(const std::string &str) :input(str) { }
    char getc() {
        if (input.fail())
            return EOF;
        ++pos;
        return (char) input.get();
    }

    void error() {
        errorPos = pos;
        fprintf(stderr, "(Syntax error at %d)\n", errorPos);
    }

    std::stringstream input;
    int pos = -1;
    int errorPos = -1;
};

int n1ql_getchar(n1ql_aux aux) {return aux->getc();}
void n1ql_error(n1ql_aux aux)  {aux->error();}
*/

char* c4query_translateN1QL(C4String n1ql,
                            C4TranslateN1QLFlags flags,
                            char** outErrorMessage,
                            unsigned* outErrorPosition,
                            unsigned* outErrorLine) C4API
{
    stringstream input(string((char*)n1ql.buf, n1ql.size));
    sInput = &input;
    sInputPos = 0;

    ParserResult result;
    sN1QLResult = &result;
    bool ok = n1ql_parse() != 0;

    if (ok) {
        alloc_slice json = result.as<MutableDict>().toJSON( (flags & kN1QLToJSON5) != 0,
                                                            (flags & kN1QLToCanonicalJSON) != 0);
        return strdup(string(json).c_str());
    } else {
        if (outErrorMessage)
            *outErrorMessage = strdup("Syntax error");
        if (outErrorPosition)
            *outErrorPosition = sInputPos;
        if (outErrorLine)
            *outErrorLine = 0;
        return nullptr;
    }
}
