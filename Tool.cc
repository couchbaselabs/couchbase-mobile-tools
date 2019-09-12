//
// Tool.cc
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

#include "Tool.hh"
#include "Logging.hh"
#include "linenoise.h"
#include <cstdio>
#include <regex>

#if !defined(_MSC_VER)
#include <unistd.h>
#include <sys/ioctl.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define STDIN_FILENO _fileno(stdin)
#define STDOUT_FILENO _fileno(stdout)
#endif


static constexpr int kDefaultLineWidth = 100;


Tool* Tool::instance;

Tool::Tool(const char* name)
    :_name(name)
{
    if(!instance) {
        instance = this;
    }
    
    linenoiseHistorySetMaxLen(100);
}

Tool::~Tool() {
    linenoiseHistoryFree();
    if (this == instance)
        instance = nullptr;
}


static bool inputIsTerminal() {
    return isatty(STDIN_FILENO) && getenv("TERM") != nullptr;
}

#ifdef _MSC_VER
typedef LONG NTSTATUS, *PNTSTATUS;
#define STATUS_SUCCESS (0x00000000)

typedef NTSTATUS (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

RTL_OSVERSIONINFOW GetRealOSVersion() {
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if ( STATUS_SUCCESS == fxPtr(&rovi) ) {
                return rovi;
            }
        }
    }
    RTL_OSVERSIONINFOW rovi = { 0 };
    return rovi;
}
#endif

static bool sOutputIsColor = false;

void Tool::enableColor() {
    if (getenv("CLICOLOR_FORCE")) {
        sOutputIsColor = true;
        return;
    }
    
    const char *term = getenv("TERM");
    if(isatty(STDOUT_FILENO)
            && term != nullptr
            && (strstr(term,"ANSI") || strstr(term,"ansi") || strstr(term,"color"))) {
        sOutputIsColor = true;
        return;
    }

#ifdef _MSC_VER
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    // Sick of this being missing for whatever reason
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
    #endif

    sOutputIsColor = GetRealOSVersion().dwMajorVersion >= 10;
    if(sOutputIsColor) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode;
        if(GetConsoleMode(hConsole, &consoleMode)) {
            SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif

}

string Tool::ansi(const char *command) {
    if (sOutputIsColor)
        return format("\033[%sm", command);
    else
        return "";
}


int Tool::terminalWidth() {
#if __APPLE__
    struct ttysize ts;
    if (ioctl(0, TIOCGSIZE, &ts) == 0 && ts.ts_cols > 0)
        return ts.ts_cols;
#endif
    return kDefaultLineWidth;
}

bool Tool::readLine(const char *cPrompt) {
    if (!inputIsTerminal())
        return dumbReadLine(cPrompt);

    string prompt = cPrompt;
    if (sOutputIsColor)
        prompt = ansiBold() + prompt + ansiReset();

    while (true) {
        char* line = linenoise(prompt.c_str());
        // Returned line and lineLength include the trailing newline, unless user typed ^D.
        if (line != nullptr && strlen(line) > 0) {
            // Got a command!
            // Add line to history so user can recall it later:
            linenoiseHistoryAdd(line);
            _argTokenizer.reset(line);
            return true;
        } else if(linenoiseKeyType() == 2) {
            cout << endl;
            return false;
        }
        
        // No command was entered, so go round again:
        cout << "Please type a command, or Ctrl-D to exit.\n";
    }
}


bool Tool::dumbReadLine(const char *prompt) {
    char inputBuffer[5000];
    while (true) {
        cout << ansiBold() << prompt << ansiReset();
        char* line = fgets(inputBuffer, sizeof(inputBuffer), stdin);
        if (!line) {
            cout << '\n';
            return false;
        }
        if (strlen(line) > 0) {
            _argTokenizer.reset(line);
            return true;
        }
        cout << "Please type a command, or Ctrl-D to exit.\n";
    }
}


string Tool::readPassword(const char *prompt) {
#if defined(_MSC_VER)
    fail("Sorry, password input is unimplemented on Windows");  //FIX //TODO
#else
    char *cpass = getpass(prompt);
    string password = cpass;
    memset(cpass, '*', strlen(cpass)); // overwrite password at known static location
    return password;
#endif
}
