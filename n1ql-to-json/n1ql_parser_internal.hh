//
//  n1ql_parser_internal.hh
//  Tools
//
//  Created by Jens Alfke on 4/11/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "n1ql_parser.h"
#include <iostream>
#include <typeinfo>

using namespace fleece;
using namespace antlrcpp;
using string = std::string;


// Adding 'Any' type to Array/Dict:


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
    else
        throw std::bad_cast();
    return array;
}

static MutableArray insertAny(MutableArray array, unsigned index, const Any &value) {
    array.insertNulls(index, 1);
    return setAny(array, index, value);
}

static MutableArray appendAny(MutableArray array, const Any &value) {
    unsigned index = array.count();
    array.appendNull();
    return setAny(array, index, value);
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
MutableArray arrayWith(Any item) {
    auto a = array();
    return appendAny(a, item);
}


// Constructing JSON operations:


static MutableArray op(const string &oper, Any op1) {
    return appendAny(arrayWith(oper.c_str()), op1);
}

static MutableArray op(const string &oper, Any op1, Any op2) {
    return appendAny(op(oper, op1), op2);
}

static MutableArray binaryOp(Any left, Any oper, Any right) {
    return op(oper.as<string>().c_str(), left, right);
}

static MutableArray unaryOp(Any oper, Any right) {
    return op(oper.as<string>().c_str(), right);
}


// String utilities:


static string unquote(const string &str, char quoteChar) {
    string quote(1, quoteChar);
    string doubleQuote(2, quoteChar);
    string result = str;
    string::size_type pos = 0;
    while (string::npos != (pos = result.find(doubleQuote, pos))) {
        result.replace(pos, 2, quote);
        pos += 1;
    }
    return result;
}
