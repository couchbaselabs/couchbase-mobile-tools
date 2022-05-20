//
// LiteCoreTool.hh
//
// Copyright © 2021 Couchbase. All rights reserved.
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
#include "c4Base.h"
#include "fleece/Fleece.hh"
#include <cctype>
#include <functional>
#include <set>
#include <string>


static inline std::string to_string(C4String s) {
    return std::string((const char*)s.buf, s.size);
}

static inline C4Slice c4str(const std::string &s) {
    return {s.data(), s.size()};
}


class LiteCoreTool : public Tool {
public:
    static LiteCoreTool* instance() {return dynamic_cast<LiteCoreTool*>(Tool::instance);}

    LiteCoreTool(const char* name)                              :Tool(name) { }
    LiteCoreTool(const Tool &parent)                            :Tool(parent) { }
    LiteCoreTool(const Tool &parent, const char *commandLine)   :Tool(parent, commandLine) { }

    void errorOccurred(const std::string &what, C4Error err) {
        std::cerr << ansiRed() << "Error";
        if (!islower(what[0]))
            std::cerr << ":";
        std::cerr << " " << what;
        if (err.code) {
            fleece::alloc_slice message = c4error_getMessage(err);
            if (message.buf)
                std::cerr << ": " << to_string(message);
            std::cerr << ansiReset() << " (" << int(err.domain) << "/" << err.code << ")";
        }
        std::cerr << ansiReset() << "\n";

        ++_errorCount;
        if (_failOnError)
            fail();
    }

    [[noreturn]] void fail(const std::string &what, C4Error err) {
        errorOccurred(what, err);
        fail();
    }

    [[noreturn]] static void fail() {Tool::fail();}
    [[noreturn]] void fail(const std::string &message) {Tool::fail(message);}

    std::string tempDirectory();

    /// Writes un-pretty-printed JSON. If docID and/or revID given, adds them as fake properties.
    void rawPrint(fleece::Value body,
                  fleece::slice docID,
                  fleece::slice revID =fleece::nullslice);

    /// Pretty-prints JSON. If docID and/or revID given, adds them as fake properties.
    void prettyPrint(fleece::Value value,
                     std::ostream &out,
                     const std::string &indent ="",
                     fleece::slice docID =fleece::nullslice,
                     fleece::slice revID =fleece::nullslice,
                     const std::set<fleece::alloc_slice> *onlyKeys =nullptr);

    static void writeSize(uint64_t n);

    /// Returns true if this string does not require quotes around it as a JSON5 dict key.
    static bool canBeUnquotedJSON5Key(fleece::slice key);

    /// Returns a string in JSON format, with double-quotes and any necessary escapes.
    static std::string quoteJSONString(fleece::slice str);

    // Pattern matching using the typical shell `*` and `?` metacharacters. A `\` escapes them.

    static bool isGlobPattern(std::string &str);
    static void unquoteGlobPattern(std::string &str);
    bool globMatch(const char *name, const char *pattern);


    bool                            _json5 {false};
};
