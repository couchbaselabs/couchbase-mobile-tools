//
//  n1ql_parser.h
//  Tools
//
//  Created by Jens Alfke on 4/10/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include "Any.h"

using namespace fleece;
using Any = antlrcpp::Any;

int n1ql_input(char *buf, size_t max_size);
int n1ql_parse();
int n1ql_parse_from(int (*)(struct _yycontext*));

using ParserResult = Any;

extern ParserResult* sN1QLResult;
