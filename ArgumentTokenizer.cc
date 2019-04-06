//
// ArgumentTokenizer.cc
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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

#include "ArgumentTokenizer.hh"
#include <sstream>
using namespace std;


void ArgumentTokenizer::reset(const char *input) {
    _input = input;
    _current = _input.c_str();
    _args.clear();
    next();
}


void ArgumentTokenizer::reset(std::vector<std::string> args) {
    _args = args;
    _current = nullptr;
    _startOfArg = nullptr;
    next();
}


bool ArgumentTokenizer::next() {
    _hasArgument = true;
    if (!_args.empty()) {
        _argument = _args[0];
        _args.erase(_args.begin());
        return true;
    }

    if (_current) {
        _startOfArg = _current;
        char quoteChar = 0;
        bool inQuote = false;
        bool forceAppend = false;
        string nextArg;
        while(*_current) {
            char c = *_current;
            _current++;
            if(c == '\r' || c == '\n') {
                continue;
            }

            if(!forceAppend) {
                if(c == '\\') {
                    forceAppend = true;
                    continue;
                } else if(c == '"' || c == '\'') {
                    if(quoteChar != 0 && c == quoteChar) {
                        inQuote = false;
                        quoteChar = 0;
                        continue;
                    } else if(quoteChar == 0) {
                        inQuote = true;
                        quoteChar = c;
                        continue;
                    }
                } else if(c == ' ' && !inQuote) {
                    if (!nextArg.empty()) {
                        _argument = nextArg;
                        return true;
                    }
                    continue;
                }
            } else {
                forceAppend = false;
            }

            nextArg.append(1, c);
        }

        if(inQuote)
            throw runtime_error("Invalid input line: Unclosed quote");
        if (forceAppend)
            throw runtime_error("Invalid input line: missing character after '\\'");

        _current = nullptr;
        if(nextArg.length() > 0) {
            _argument = nextArg;
            return true;
        }
    }
    reset();
    return false;
}


void ArgumentTokenizer::reset() {
    _args.clear();
    _input.clear();
    _current = nullptr;
    _hasArgument = false;
    _startOfArg = nullptr;
    _argument = "";
}


string ArgumentTokenizer::restOfInput() {
    string result;
    if (_startOfArg) {
        result = string(_startOfArg);
    } else if (_hasArgument) {
        stringstream rest;
        rest << _argument;
        for (const string &arg : _args)
            rest << ' ' << arg;
        result = rest.str();
    }
    reset();
    return result;
}
