//
// CBRemoteTool.hh
//
// Copyright © 2022 Couchbase. All rights reserved.
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
#include "c4.hh"    // LiteCore C++ API


class CBRemoteCommand;


class CBRemoteTool : public LiteCoreTool {
public:
    CBRemoteTool()
    :LiteCoreTool("cblite")
    { }

    CBRemoteTool(const CBRemoteTool &parent)
    :LiteCoreTool(parent)
    ,_db(parent._db)
    { }

    ~CBRemoteTool();

    // Main handlers:
    void usage() override;
    int run() override;

    static bool isDatabaseURL(const std::string&);

protected:
    virtual void addLineCompletions(ArgumentTokenizer&, std::function<void(const std::string&)>) override;

    void openDatabase(const std::string &url, bool interactive);

    std::unique_ptr<CBRemoteCommand> subcommand(const std::string &name);

    virtual void helpCommand();

    void displayVersion();

    [[noreturn]] virtual void failMisuse(const std::string &message) override {
        std::cerr << "Error: " << message << std::endl;;
        std::cerr << "Please run `cbremote help` for usage information." << std::endl;
        fail();
    }

    fleece::Retained<C4ConnectedClient>  _db;
    std::string                 _dbURL;
    bool                        _shouldCloseDB {false};
};
