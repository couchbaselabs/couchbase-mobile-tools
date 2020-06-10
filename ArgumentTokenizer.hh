//
// ArgumentTokenizer.hh
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

#pragma once
#include <string>
#include <vector>

class ArgumentTokenizer {
public:
    ArgumentTokenizer()
    { }

    ArgumentTokenizer(const ArgumentTokenizer &other)
    :_args(other._args)
    ,_input(other._input)
    ,_hasArgument(other._hasArgument)
    ,_argument(other._argument)
    {
        if (other._current) {
            size_t currentPos = other._current - other._input.c_str();
            _current = &_input[currentPos];
        }
        if (other._startOfArg) {
            size_t startOfArgPos = other._startOfArg - other._input.c_str();
            _startOfArg = &_input[startOfArgPos];
        }
    }

    void reset();
    void reset(const char *input);
    void reset(std::vector<std::string> args);

    bool hasArgument() const                    {return _hasArgument;}
    const std::string& argument() const         {return _argument;}
    bool next();

    std::string restOfInput();

    bool tokenize(const char *input, std::vector<std::string> &outArgs);

private:
    std::vector<std::string> _args;
    std::string _input;
    const char* _current {nullptr};
    const char* _startOfArg {nullptr};
    bool _hasArgument;
    std::string _argument;
};

