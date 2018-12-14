//
//  cbl_logcat.cc
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

#include "LogDecoder.hh"
#include "Tool.hh"
#include <vector>
#include <fstream>
using namespace std;

class CBLLogCat : public Tool
{
public:
    void usage();
    int run();
private:
    void logcatUsage();
    void logcat();
    void helpCommand();

    static const FlagSpec kSubcommands[];

    string _currentCommand;
    bool _showHelp {false};
};

const Tool::FlagSpec CBLLogCat::kSubcommands[] = {
    {"help",    (FlagHandler)&CBLLogCat::helpCommand},
    {"logcat",  (FlagHandler)&CBLLogCat::logcat},
    {nullptr, nullptr}
};

int main(int argc, const char * argv[]) {
    CBLLogCat tool;
    return tool.main(argc, argv);
}

void CBLLogCat::usage() {
    cerr <<
    ansiBold() << "cbl_logcat: Couchbase Lite / LiteCore log decoder\n" << ansiReset() <<
    "Usage: cbl_logcat help " << it("[SUBCOMMAND]") << "\n"
    "       cbl_logcat logcat " << it("LOGPATH") << "\n"
    "For information about subcommand parameters/flags, run `cbl_logcat help SUBCOMMAND`.\n"
    ;
}

int CBLLogCat::run() {
    c4log_setCallbackLevel(kC4LogWarning);
    if (argCount() == 0) {
        cerr << ansiBold()
             << "cbl_logcat: Couchbase Lite / LiteCore log decoder\n" << ansiReset() 
             << "Missing subcommand.\n"
             << "For a list of subcommands, run " << ansiBold() << "cbl_logcat help" << ansiReset() << ".\n";
        fail();
    }

    string cmd = nextArg("subcommand or database path");
    if (!processFlag(cmd, kSubcommands)) {
        _currentCommand = "";
        failMisuse(format("Unknown subcommand '%s'", cmd.c_str()));
    }

    return 0;
}

void CBLLogCat::logcatUsage() {
    cerr << ansiBold();
    cerr << "cbl_logcat logcat" << ' ' << ansiItalic() << "LOGFILE" << ansiReset() << '\n';
    cerr <<
    "  Converts a binary log file to text and writes it to stdout\n"
    ;
}

void CBLLogCat::logcat() {
    // Read params:
    processFlags(nullptr);
    if (_showHelp) {
        logcatUsage();
        return;
    }
    string logPath = nextArg("log file path");

    vector<string> kLevels = {"***", "", "",
        ansiBold() + ansiRed() + "WARNING" + ansiReset(),
        ansiBold() + ansiRed() + "ERROR" + ansiReset()};

    ifstream in(logPath, ifstream::in | ifstream::binary);
    if (!in)
        fail(format("Couldn't open '%s'", logPath.c_str()));
    in.exceptions(std::ifstream::badbit);
    
    LogDecoder decoder(in);
    decoder.decodeTo(cout, kLevels);
}

void CBLLogCat::helpCommand() {
    if (argCount() > 0) {
        _showHelp = true; // forces command to show help and return
        string cmd = nextArg("subcommand");
        if (!processFlag(cmd, kSubcommands))
            cerr << format("Unknown subcommand '%s'\n", cmd.c_str());
    } else {
        usage();
    }
}
