//
// Tool.hh
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
#include "fleece/Fleece.hh"
#include "StringUtil.hh"
#include "ArgumentTokenizer.hh"
#include <iostream>
#include <string>
#include <deque>
#include <algorithm>

#ifndef CBLTOOL_NO_C_API
#include "c4.hh"
#endif

#ifdef CMAKE
#include "config.h"
#else
#define TOOLS_VERSION_STRING "0.0.0"
#endif

using namespace std;
using namespace fleece;
using namespace litecore;


#ifndef CBLTOOL_NO_C_API
static inline string to_string(C4String s) {
    return string((const char*)s.buf, s.size);
}

static inline C4Slice c4str(const string &s) {
    return {s.data(), s.size()};
}
#endif

class Tool {
public:
    Tool(const char* name);
    virtual ~Tool();

    static Tool* instance;

    virtual int main(int argc, const char * argv[]) {
        try {
            if (getenv("CLICOLOR"))
                enableColor();
            _toolPath = string(argv[0]);
            vector<string> args;
            for(int i = 1; i < argc; ++i)
                args.push_back(argv[i]);
            _argTokenizer.reset(args);
            processFlags(initialFlags());
            return run();
        } catch (const fail_error &) {
            return 1;
        } catch (const std::exception &x) {
            errorOccurred(format("Uncaught C++ exception: %s", x.what()));
            return 1;
        } catch (...) {
            errorOccurred("Uncaught unknown C++ exception");
            return 1;
        }
    }

    virtual void usage() = 0;

    int verbose() const                         {return _verbose;}

#pragma mark - ERRORS / FAILURE:

    // A placeholder exception thrown by fail() and caught in run() or a CLI loop
    class fail_error : public runtime_error {
    public:
        fail_error() :runtime_error("fail called") { }
    };

    void errorOccurred(const string &what
#ifndef CBLTOOL_NO_C_API
                       , C4Error err ={}
#endif
                                        ){
        cerr << "Error";
        if (!islower(what[0]))
            cerr << ":";
        cerr << " " << what;
#ifndef CBLTOOL_NO_C_API
        if (err.code) {
            alloc_slice message = c4error_getMessage(err);
            if (message.buf)
                cerr << ": " << to_string(message);
            cerr << " (" << err.domain << "/" << err.code << ")";
        }
#endif
        cerr << "\n";

        if (_failOnError)
            fail();
    }

    [[noreturn]] static void fail() {
        throw fail_error();
    }

    [[noreturn]] void fail(const string &message) {
        errorOccurred(message);
        fail();
    }


#ifndef CBLTOOL_NO_C_API
    [[noreturn]] void fail(const string &what, C4Error err) {
        errorOccurred(what, err);
        fail();
    }
#endif

    [[noreturn]] virtual void failMisuse(const string &message) {
        cerr << "Error: " << message << "\n";
        usage();
        fail();
    }

#pragma mark - I/O:

    /** Interactively reads a command from the terminal, preceded by the prompt.
        If it returns true, the command has been parsed into the argument buffer just like the
        initial command line.
        If it returns false, the user has decided to end the session (probably by hitting ^D.) */
    bool readLine(const char *prompt);

    /** Reads a password from the terminal without echoing it. */
    string readPassword(const char *prompt);

    alloc_slice readFile(const string &path);

    enum TerminalType {
        kTTY,
        kColorTTY,
        kIDE,
        kColorIDE,
        kFile,
        kOther,
    };

    TerminalType terminalType();

    int terminalWidth();

    string ansi(const char *command);
    string ansiBold()                   {return ansi("1");}
    string ansiDim()                    {return ansi("2");}
    string ansiItalic()                 {return ansi("3");}
    string ansiUnderline()              {return ansi("4");}
    string ansiRed()                    {return ansi("31");}
    string ansiReset()                  {return ansi("0");}

    string bold(const char *str)        {return ansiBold() + str + ansiReset();}
    string it(const char *str)          {return ansiItalic() + str + ansiReset();}

    string spaces(int n)                {return string(max(n, 1), ' ');}

protected:

    /** Top-level action, called after flags are processed.
     Return value will be returned as the exit status of the process. */
    virtual int run() =0;

#pragma mark - ARGUMENT HANDLING:

    typedef void (Tool::*FlagHandler)();
    struct FlagSpec {const char *flag; FlagHandler handler;};

    /** Returns the specs of the top-level flags to be handled when the tool starts.
        May return null if there are no such flags. */
    virtual const FlagSpec* initialFlags() {
        return nullptr;
    }

    bool hasArgs() const {
        return _argTokenizer.hasArgument();
    }

    /** Returns the next argument without consuming it, or "" if there are no remaining args. */
    string peekNextArg() {
        return _argTokenizer.argument();
    }

    /** Returns & consumes the next arg, or fails if there are none. */
    string nextArg(const char *what) {
        if (!_argTokenizer.hasArgument())
            failMisuse(format("Missing argument: expected %s", what));
        string arg = _argTokenizer.argument();
        _argTokenizer.next();
        return arg;
    }

    string restOfInput(const char *what) {
        if (!_argTokenizer.hasArgument())
            failMisuse(format("Missing argument: expected %s", what));
        return _argTokenizer.restOfInput();
    }

    /** Call when there are no more arguments to read. Will fail if there are any args left. */
    void endOfArgs() {
        if (_argTokenizer.hasArgument())
            fail(format("Unexpected extra arguments, starting with '%s'",
                        _argTokenizer.argument().c_str()));
    }


    /** Consumes arguments as long as they begin with "-".
        Each argument is looked up in the list of FlagSpecs and the matching one's handler is
        called. If there is no match, fails. */
    virtual void processFlags(const FlagSpec specs[]) {
        while(true) {
            string flag = peekNextArg();
            if (flag.empty() || !hasPrefix(flag, "-") || flag.size() > 20)
                return;
            _argTokenizer.next();

            if (flag == "--")
                return;
            if (!processFlag(flag, specs)) {
                if (flag == "--help") {
                    usage();
                    exit(0);
                } else if (flag == "--verbose" || flag == "-v") {
                    ++_verbose;
                } else if (flag == "--color") {
                    enableColor();
                } else if (flag == "--version") {
                    cout << _name << " " << TOOLS_VERSION_STRING << endl << endl;
                    exit(0);
                } else {
                    fail(string("Unknown flag ") + flag);
                }
            }
        }
    }

    /** Subroutine of processFlags; looks up one flag and calls its handler, or returns false. */
    bool processFlag(const string &flag, const FlagSpec specs[]) {
        if (!specs)
            return false;
        for (const FlagSpec *spec = specs; spec->flag; ++spec) {
            if (flag == string(spec->flag)) {
                (this->*spec->handler)();
                return true;
            }
        }
        return false;
    }

    void verboseFlag() {
        ++_verbose;
    }

    bool _failOnError {false};

    void fixUpPath(string &path) {
#ifndef _MSC_VER
        if (hasPrefix(path, "~/")) {
            path.erase(path.begin(), path.begin()+1);
            path.insert(0, getenv("HOME"));
        }
#endif
    }

private:
    void enableColor();
    static const char* promptCallback(struct editline *e);
    bool dumbReadLine(const char *prompt);

    string _toolPath;
    int _verbose {0};
    std::string _editPrompt;
    ArgumentTokenizer _argTokenizer;
    const char* _name;
};
