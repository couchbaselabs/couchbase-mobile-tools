//
// CBLiteTool.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#include "LiteCoreTool.hh"
#include "LiteCorePolyfill.hh"
#include "FilePath.hh"
#include "StringUtil.hh"
#include <exception>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <vector>

// Unofficial LiteCore C++ API helpers; in dev their header has been renamed
#if __has_include("tests/c4CppUtils.hh")
#   include "tests/c4CppUtils.hh"       // dev branch
#else
#   include "c4.hh"                     // master branch (as of May 2021); TODO: remove after merge
#   include "c4Transaction.hh"
#endif


class CBLiteCommand;


class CBLiteTool : public LiteCoreTool {
public:
    CBLiteTool()
    :LiteCoreTool("cblite")
    { }

    CBLiteTool(const CBLiteTool &parent)
    :LiteCoreTool(parent)
    ,_db(c4::retainRef(parent._db))
    ,_dbFlags(parent._dbFlags)
    { }

    ~CBLiteTool();

    // Main handlers:
    void usage() override;
    int run() override;

    static std::pair<std::string,std::string> splitDBPath(const std::string &path);
    static bool isDatabasePath(const std::string &path);
    static bool isDatabaseURL(const std::string&);

protected:
    virtual void addLineCompletions(ArgumentTokenizer&, std::function<void(const std::string&)>) override;

    void openDatabase(std::string path, bool interactive);
    void openDatabaseFromURL(const std::string &url);

    std::unique_ptr<CBLiteCommand> subcommand(const std::string &name);

    virtual void helpCommand();

    void displayVersion();

    [[noreturn]] virtual void failMisuse(const std::string &message) override {
        std::cerr << "Error: " << message << std::endl;;
        std::cerr << "Please run `cblite help` for usage information." << std::endl;
        fail();
    }

    c4::ref<C4Database>   _db;
    bool                  _shouldCloseDB {false};
    C4DatabaseFlags       _dbFlags {kC4DB_ReadOnly | kC4DB_NoUpgrade};
    bool                  _dbNeedsPassword {false};
};
