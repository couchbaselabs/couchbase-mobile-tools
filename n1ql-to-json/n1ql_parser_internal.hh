//
//  n1ql_parser_internal.hh
//  Tools
//
//  Created by Jens Alfke on 4/11/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

// This header is included by the generated parser (n1ql.cc),
// so its contents are available to actions in the grammar file (n1ql.leg).

#pragma once
#include "n1ql_parser.h"
#include "Any.h"
#include <sstream>
#include <typeinfo>

//#define YY_DEBUG

#define YY_CTX_LOCAL
#define YY_CTX_MEMBERS  std::stringstream *stream;

#define YY_INPUT(ctx, buf, result, max_size) ((result)=n1ql_input(ctx, buf, max_size))
static int n1ql_input(struct _yycontext *ctx, char *buf, size_t max_size);

#define YY_PARSE(T) static T
#define YYPARSE     n1ql_parse
#define YYPARSEFROM n1ql_parse_from

#define YYSTYPE Any


using namespace fleece;
using namespace antlrcpp;
using string = std::string;


// Adding 'Any' values to Array/Dict:


static MutableDict setAny(MutableDict dict, slice key, const Any &value) {
    if (value.isNull())
        return dict;
    if (value.is<MutableArray>())
        dict.set(key, value.as<MutableArray>());
    else if (value.is<MutableDict>())
        dict.set(key, value.as<MutableDict>());
    else if (value.is<Value>())
        dict.set(key, value.as<Value>());
    else if (value.is<string>())
        dict.set(key, value.as<string>().c_str());
    else if (value.is<long long>())
        dict.set(key, value.as<long long>());
    else if (value.is<double>())
        dict.set(key, value.as<double>());
    else if (value.is<bool>())
        dict.set(key, value.as<bool>());
    else if (value.is<Null>())
        dict.setNull(key);
    else
        throw std::bad_cast();
    return dict;
}

static MutableArray setAny(MutableArray array, unsigned index, const Any &value) {
    assert(!value.isNull());
    if (value.is<MutableArray>())
        array.set(index, value.as<MutableArray>());
    else if (value.is<MutableDict>())
        array.set(index, value.as<MutableDict>());
    else if (value.is<Value>())
        array.set(index, value.as<Value>());
    else if (value.is<string>())
        array.set(index, value.as<string>().c_str());
    else if (value.is<long long>())
        array.set(index, value.as<long long>());
    else if (value.is<double>())
        array.set(index, value.as<double>());
    else if (value.is<bool>())
        array.set(index, value.as<bool>());
    else if (value.is<Null>())
        array.setNull(index);
    else
        throw std::bad_cast();
    return array;
}

static MutableArray insertAny(MutableArray array, unsigned index, const Any &value) {
    array.insertNulls(index, 1);
    return setAny(array, index, value);
}

static MutableArray appendAny(MutableArray array, const Any &value) {
    return insertAny(array, array.count(), value);
}


// Constructing arrays:


static inline MutableArray array() {
    return MutableArray::newArray();
}

template <class T>
static MutableArray arrayWith(T item) {
    auto a = array();
    a.append(item);
    return a;
}

template <>
MutableArray arrayWith(string item) {
    auto a = array();
    a.append(item.c_str());
    return a;
}

template <>
MutableArray arrayWith(Any item) {
    auto a = array();
    return appendAny(a, item);
}

template <class T>
static MutableDict dictWith(slice key, T item) {
    auto d = MutableDict::newDict();
    d.set(key, item);
    return d;
}

template <>
MutableDict dictWith(slice key, string item) {
    auto d = MutableDict::newDict();
    d.set(key, item.c_str());
    return d;
}

template <>
MutableDict dictWith(slice key, Any item) {
    auto d = MutableDict::newDict();
    setAny(d, key, item);
    return d;
}


// Constructing JSON operations:


static inline MutableArray op(const string &oper) {
    return arrayWith(oper.c_str());
}

static MutableArray op(const string &oper, Any op1) {
    return appendAny(op(oper), op1);
}

static MutableArray op(const string &oper, Any op1, Any op2) {
    return appendAny(op(oper, op1), op2);
}

static MutableArray op(const string &oper, Any op1, Any op2, Any op3) {
    return appendAny(op(oper, op1, op2), op3);
}

static MutableArray binaryOp(Any left, Any oper, Any right) {
    return op(oper.as<string>().c_str(), left, right);
}

static MutableArray unaryOp(Any oper, Any right) {
    return op(oper.as<string>().c_str(), right);
}


static bool hasPathPrefix(slice path, slice prefix) {
    return path.hasPrefix(prefix) && (path.size == prefix.size ||
                                      (path[prefix.size] == '.' || path[prefix.size] == '['));
}


// Variable substitution:


static void _substituteVariable(slice varWithDot, MutableArray expr) {
    int index = 0;
    for (Array::iterator i(expr); i; ++i) {
        Value item = i.value();
        if (index++ == 0) {
            if (hasPathPrefix(item.asString(), varWithDot)) {
                // Change '.xxx' to '?xxx':
                string op = string(item.asString());
                op[0] = '?';
                expr[0] = op.c_str();
            }
        } else {
            auto operation = item.asArray().asMutable();
            if (operation)
                _substituteVariable(varWithDot, operation); // recurse
        }
    }
}

// Postprocess an expression by changing references to 'var' from a property to a variable.
static void substituteVariable(const string &var, MutableArray expr) {
    _substituteVariable(slice("." + var), expr);
}


// String utilities:


static void uppercase(string &str) {
    for (char &c : str)
        c = toupper(c);
}


static void replace(std::string &str, const std::string &oldStr, const std::string &newStr) {
    string::size_type pos = 0;
    while (string::npos != (pos = str.find(oldStr, pos))) {
        str.replace(pos, oldStr.size(), newStr);
        pos += newStr.size();
    }
}

static string trim (const char *input) {
    while (isspace(*input))
        ++input;
    const char *last = input + strlen(input) - 1;
    while (last >= input && isspace(*last))
        --last;
    return string(input, last-input+1);
}

static string unquote(string str, char quoteChar) {
    replace(str, string(2, quoteChar), string(1, quoteChar));
    return str;
}

static string quoteProperty(string prop) {
    replace(prop, ".", "\\.");
    replace(prop, "$", "\\$");
    prop.replace(0, 0, ".");
    return prop;
}


// Recognizing reserved words:


static const char* kReservedWords[] = {
    "AND",  "ANY",  "AS",  "ASC",  "BETWEEN",  "BY",  "CASE",  "CROSS",  "DESC",  "DISTINCT",
    "ELSE",  "END",  "EVERY",  "FALSE",  "FROM",  "GROUP",  "HAVING",  "IN",  "INNER",  "IS",
    "JOIN",  "LEFT",  "LIKE",  "LIMIT",  "MATCH",  "META",  "MISSING",  "NATURAL",  "NOT",
    "NULL",  "MISSING",  "OFFSET",  "ON",  "OR",  "ORDER",  "OUTER",  "REGEX",  "RIGHT",
    "SATISFIES",  "SELECT",  "THEN",  "TRUE",  "USING",  "WHEN",  "WHERE",
    "COLLATE",
    nullptr
};

static const char* kFunctions[] = {         // (copied from LiteCore's QueryParserTables.hh)
    // Array:
    "array_avg",  "array_contains",  "array_count",  "array_ifnull",  "array_length",  "array_max",
    "array_min",  "array_of",  "array_sum",
    // Comparison:  (SQLite min and max are used in non-aggregate form here)
    "greatest",  "least",
    // Conditional (unknowns):
    "ifmissing",  "ifnull",  "ifmissingornull",  "missingif",  "nullif",
    // Dates/times:
    "millis_to_str",  "millis_to_utc",  "str_to_millis",  "str_to_utc",
    // Math:
    "abs",  "acos",  "asin",  "atan",  "atan2",  "ceil",  "cos",  "degrees",  "e",  "exp",
    "floor",  "ln",  "log",  "pi",  "power",  "radians",  "round",  "sign",  "sin",  "sqrt",
    "tan",  "trunc",
    // Patterns:
    "regexp_contains",  "regexp_like",  "regexp_position",  "regexp_replace",
    // Strings:
    "contains",  "length",  "lower",  "ltrim",  "rtrim",  "trim",  "upper",
    // Types:
    "isarray",  "isatom",  "isboolean",  "isnumber",  "isobject",  "isstring",  "type",  "toarray",
    "toatom",  "toboolean",  "tonumber",  "toobject",  "tostring",
    // FTS (not standard N1QL):
    "rank",
    // Aggregate functions:
    "avg",  "count",  "max",  "min",  "sum",
    // Predictive query:
    "prediction",  "euclidean_distance",  "cosine_distance",
    nullptr
};

static bool findIdentifier(const char *ident, const char* list[]) {
    for (int i = 0; list[i]; ++i)
        if (strcasecmp(ident, list[i]) == 0)
            return true;
    return false;
}

static inline bool isReservedWord(const char *ident) {
    return findIdentifier(ident, kReservedWords);
}

static inline bool isFunction(const char *fn) {
    return findIdentifier(fn, kFunctions);
}


// Collation modes:

static void extendCollate(MutableArray expr, string collation) {
    auto coll = expr[1].asDict().asMutable();
    assert(coll);
    uppercase(collation);
    bool value = (collation.substr(0,2) != "NO");
    if (!value)
        collation = collation.substr(2);
    coll[slice(collation)] = value;
}

static MutableArray collateOp(MutableArray expr, string collation) {
    auto collate = op("COLLATE", MutableDict::newDict(), expr);
    extendCollate(collate, collation);
    return collate;
}
