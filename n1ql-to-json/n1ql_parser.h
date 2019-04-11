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

struct _yycontext;

int n1ql_parse(void);
int n1ql_parse_from(int (*)(struct _yycontext*));
struct _yycontext* yyrelease(struct _yycontext *);

int n1ql_input(char *buf, size_t max_size);

//#define YY_DEBUG

#define YYPARSE     n1ql_parse
#define YYPARSEFROM n1ql_parse_from
#define YY_INPUT(buf, result, max_size) ((result)=n1ql_input(buf, max_size))

using ParserResult = Any;
#define YYSTYPE ParserResult

extern ParserResult* sN1QLResult;
