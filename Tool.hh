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
#include <functional>
#include <iostream>
#include <string>
#include <deque>
#include <algorithm>

#ifdef CMAKE
#include "config.h"
#else
#define TOOLS_VERSION_STRING "0.0.0"
#endif


class Tool {
public:
    Tool(const char* name);
    virtual ~Tool();

    static Tool* instance;

    /** Entry point for a Tool. */
    virtual int main(int argc, const char * argv[]) {
        try {
            if (getenv("CLICOLOR"))
                enableColor();
            _toolPath = std::string(argv[0]);
            std::vector<std::string> args;
            for(int i = 1; i < argc; ++i)
                args.push_back(argv[i]);
            _argTokenizer.reset(args);
            return run();
        } catch (const exit_error &x) {
            return x.status;
        } catch (const fail_error &) {
            return 1;
        } catch (const std::exception &x) {
            errorOccurred(litecore::format("Uncaught C++ exception: %s", x.what()));
            return 1;
        } catch (...) {
            errorOccurred("Uncaught unknown C++ exception");
            return 1;
        }
    }

    Tool(const Tool &parent)
    :_verbose(parent._verbose)
    ,_toolPath(parent._toolPath)
    ,_argTokenizer(parent._argTokenizer)
    ,_name(parent._name)
    { }

    
    Tool(const Tool &parent, const char *commandLine)
    :_verbose(parent._verbose)
    ,_toolPath(parent._toolPath)
    ,_name(parent._name)
    {
        _argTokenizer.reset(commandLine);
    }


    const std::string& name() const                  {return _name;}
    void setName(const std::string &name)            {_name = name;}

    virtual void usage() = 0;

    int verbose() const                         {return _verbose;}
    void setVerbose(int level)                  {_verbose = level;}

#pragma mark - ERRORS / FAILURE:

    // A placeholder exception thrown by fail() and caught in run() or a CLI loop
    class fail_error : public std::runtime_error {
    public:
        fail_error() :runtime_error("fail called") { }
    };

    // A placeholder exception to exit the tool or subcommand
    class exit_error : public std::runtime_error {
    public:
        exit_error(int s) :runtime_error("(exiting)"), status(s) { }
        int const status;
    };

    static void exit(int status) {
        throw exit_error(status);
    }

    void errorOccurred(const std::string &what){
        std::cerr << "Error";
        if (!islower(what[0]))
            std::cerr << ":";
        std::cerr << " " << what << "\n";

        ++_errorCount;
        if (_failOnError)
            fail();
    }

    [[noreturn]] static void fail() {
        throw fail_error();
    }

    [[noreturn]] void fail(const std::string &message) {
        errorOccurred(message);
        fail();
    }

    [[noreturn]] virtual void failMisuse(const std::string &message) {
        std::cerr << "Error: " << message << "\n";
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
    std::string readPassword(const char *prompt);

    /** Reads the contents of a file into memory. */
    fleece::alloc_slice readFile(const std::string &path);

    /** Called during readLine when the user hits the Tab key.*/
    virtual void addLineCompletions(ArgumentTokenizer&, std::function<void(const std::string&)>) { }

    enum TerminalType {
        kTTY,
        kColorTTY,
        kIDE,
        kColorIDE,
        kFile,
        kOther,
    };

    TerminalType terminalType();

    static unsigned terminalWidth();

    /** The character width of a string when displayed in a terminal.
        This understands UTF-8, and knows which Unicode chars are double-width in terminals. */
    static unsigned terminalStringWidth(fleece::slice str);

    std::string ansi(const char *command);
    std::string ansiBold()                   {return ansi("1");}
    std::string ansiDim()                    {return ansi("2");}
    std::string ansiItalic()                 {return ansi("3");}
    std::string ansiUnderline()              {return ansi("4");}
    std::string ansiRed()                    {return ansi("31");}
    std::string ansiReset()                  {return ansi("0");}

    std::string bold(const char *str)        {return ansiBold() + str + ansiReset();}
    std::string it(const char *str)          {return ansiItalic() + str + ansiReset();}

    std::string spaces(int n)                {return std::string(std::max(n, 1), ' ');}

protected:

    /** Top-level action, called after flags are processed.
     Return value will be returned as the exit status of the process. */
    virtual int run() =0;

#pragma mark - ARGUMENT HANDLING:

    typedef litecore::function_ref<void()> FlagHandler;
    struct FlagSpec {const char *flag; FlagHandler handler;};

    bool hasArgs() const {
        return _argTokenizer.hasArgument();
    }

    /** Returns the next argument without consuming it, or "" if there are no remaining args. */
    std::string peekNextArg() {
        return _argTokenizer.argument();
    }

    /** Returns & consumes the next arg, or fails if there are none. */
    std::string nextArg(const char *what) {
        if (!_argTokenizer.hasArgument())
            failMisuse(litecore::format("Missing argument: expected %s", what));
        std::string arg = _argTokenizer.argument();
        _argTokenizer.next();
        return arg;
    }

    /** If the next arg matches the given string, consumes it and returns true. */
    bool matchArg(const char *matchArg) {
        if (_argTokenizer.argument() != matchArg)
            return false;
        _argTokenizer.next();
        return true;
    }

    std::string restOfInput(const char *what) {
        if (!_argTokenizer.hasArgument())
            failMisuse(litecore::format("Missing argument: expected %s", what));
        return _argTokenizer.restOfInput();
    }

    /** Call when there are no more arguments to read. Will fail if there are any args left. */
    void endOfArgs() {
        if (_argTokenizer.hasArgument())
            fail(litecore::format("Unexpected extra arguments, starting with '%s'",
                        _argTokenizer.argument().c_str()));
    }

    /** Returns the final argument.
        Side effect: rewinds args back to the beginning. */
    std::string lastArg() {
        std::string arg;
        while (hasArgs())
            arg = nextArg("");
        rewindArgs();
        return arg;
    }

    /** Un-reads all arguments, returning back to the beginning. */
    void rewindArgs() {
        _argTokenizer.rewind();
    }


    /** Consumes arguments as long as they begin with "-".
        Each argument is looked up in the list of FlagSpecs and the matching one's handler is
        called. If there is no match, fails. */
    virtual void processFlags(std::initializer_list<FlagSpec> specs) {
        while(true) {
            std::string flag = peekNextArg();
            if (flag.empty() || !litecore::hasPrefix(flag, "-") || flag.size() > 20)
                return;
            _argTokenizer.next();

            if (flag == "--")
                return;  // marks end of flags
            if (!processFlag(flag, specs)) {
                // Flags all subcommands accept:
                if (flag == "--help") {
                    usage();
                    exit(0);
                } else if (flag == "--verbose" || flag == "-v") {
                    ++_verbose;
                } else if (flag == "--color") {
                    enableColor();
                } else if (flag == "--version") {
                    std::cout << _name << " " << TOOLS_VERSION_STRING << std::endl << std::endl;
                    exit(0);
                } else {
                    fail(std::string("Unknown flag ") + flag);
                }
            }
        }
    }

    /** Subroutine of processFlags; looks up one flag and calls its handler, or returns false. */
    virtual bool processFlag(const std::string &flag,
                             const std::initializer_list<FlagSpec> &specs)
    {
        for (auto &spec : specs) {
            if (flag == std::string(spec.flag)) {
                spec.handler();
                return true;
            }
        }
        return false;
    }

    void verboseFlag() {
        ++_verbose;
    }

    bool _failOnError {false};
    unsigned _errorCount {0};

    static void fixUpPath(std::string &path) {
#ifndef _MSC_VER
        if (litecore::hasPrefix(path, "~/")) {
            path.erase(path.begin(), path.begin()+1);
            path.insert(0, getenv("HOME"));
        }
#endif
    }

    static std::string fixedUpPath(std::string path) {
        fixUpPath(path);
        return path;
    }

protected:
    int _verbose {0};

private:
    void enableColor();
    static void initReadLine();

    std::string _toolPath;
    std::string _name;
    ArgumentTokenizer _argTokenizer;
};
