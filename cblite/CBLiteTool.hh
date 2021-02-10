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
#include "Tool.hh"
#include "c4.h"
#include "c4.hh"
#include "FilePath.hh"
#include "StringUtil.hh"
#include <exception>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <vector>

class CBLiteCommand;


class CBLiteTool : public Tool {
public:
    CBLiteTool() : Tool("cblite")
    { }

    CBLiteTool(const CBLiteTool &parent)
    :Tool(parent)
    ,_db(c4::retainRef(parent._db))
    ,_dbFlags(parent._dbFlags)
    ,_interactive(parent._interactive)
    { }

    // Main handlers:
    void usage() override;
    int run() override;

    static std::pair<std::string,std::string> splitDBPath(const std::string &path);
    static bool isDatabasePath(const std::string &path);

protected:
    virtual void addLineCompletions(ArgumentTokenizer&, std::function<void(const std::string&)>) override;

    void openDatabase(std::string path);
    void openDatabaseFromNextArg();
    void openWriteableDatabaseFromNextArg();

    std::unique_ptr<CBLiteCommand> subcommand(const std::string &name);

    // shell command
    void shell();
    void runInteractively();
    void helpCommand();
    void quitCommand();

    void displayVersion();
    void writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs ="");

    [[noreturn]] virtual void failMisuse(const std::string &message) override {
        std::cerr << "Error: " << message << std::endl;;
        std::cerr << "Please run `cblite help` for usage information." << std::endl;
        fail();
    }

    c4::ref<C4Database> _db;
    C4DatabaseFlags     _dbFlags {kC4DB_SharedKeys | kC4DB_NonObservable | kC4DB_ReadOnly};
    bool                _dbNeedsPassword {false};
    bool                _interactive {false};
};
