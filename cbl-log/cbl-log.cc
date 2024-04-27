//
//  cbl-log.cc
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
#include "Logging.hh"
#include "Tool.hh"
#include "MultiLogDecoder.hh"
#include <vector>
#include <fstream>
#include <filesystem>
using namespace std;
using namespace litecore;
namespace fs = std::filesystem;

class CBLLogCat : public Tool
{
public:
    CBLLogCat() : Tool("cbl-log") {}
    void usage() override;
    int run() override;

    void writeLogCSV(MultiLogDecoder& decoder, ostream& out);
    void writeLog(MultiLogDecoder& decoder, ostream& out);
private:
    void logcatUsage();
    void logcat();
    void helpCommand();

    string                  _currentCommand;
    bool                    _showHelp {false};

    // logcat options
    bool                    _full{ false };
    bool                    _csv{ false };
    string                  _outputFile;
};

int main(int argc, const char * argv[]) {
    CBLLogCat tool;
    return tool.main(argc, argv);
}

void CBLLogCat::writeLog(MultiLogDecoder& decoder, ostream& out) {
    vector<string> levels = { "***", "", "", "WARNING", "ERROR" };
    if (_outputFile.empty()) {
        levels[3] = ansiBold() + ansiRed() + levels[3] + ansiReset();
        levels[4] = ansiBold() + ansiRed() + levels[4] + ansiReset();
    }

    LogIterator::Timestamp start = decoder.startTime();
    if (_full)
        start = decoder.fullStartTime();

    while (decoder.next()) {
        if (decoder.timestamp() < start)
            continue;

        out << ansiDim();
        LogIterator::writeISO8601DateTime(decoder.timestamp(), out);
        out << ansiReset() << " ";

        auto level = decoder.level();
        string levelName;
        if (level >= 0 && level < levels.size())
            levelName = levels[level];
        LogIterator::writeHeader(levelName, decoder.domain(), out);
        decoder.decodeMessageTo(out);
        out << '\n';
    }
}


void CBLLogCat::writeLogCSV(MultiLogDecoder& decoder, ostream& out) {
    // CSV format as per https://tools.ietf.org/html/rfc4180

    vector<string> levels = { "debug", "verbose", "info", "warning", "error" };

    LogIterator::Timestamp start = decoder.startTime();
    if (_full)
        start = decoder.fullStartTime();

    out << "Time,Level,Domain,Message\r\n";

    while (decoder.next()) {
        if (decoder.timestamp() < start)
            continue;

        LogIterator::writeISO8601DateTime(decoder.timestamp(), out);
        out << ',';

        auto level = decoder.level();
        if (level >= 0 && level < levels.size())
            out << levels[level];
        else
            out << int(level);

        out << ',' << decoder.domain() << ',';

        string msg = decoder.readMessage();
        replace(msg, "\"", "\"\"");

        out << '"' << msg << "\"\r\n";
    }
}

void CBLLogCat::usage() {
    cerr <<
    ansiBold() << "cbl-log: Couchbase Lite / LiteCore log decoder\n" << ansiReset() <<
    "Usage: cbl-log help " << it("[SUBCOMMAND]") << "\n"
    "       cbl-log logcat " << it("LOG_FILE [OUTPUT_FILE]") << "]\n"
    "For information about subcommand parameters/flags, run `cbl-log SUBCOMMAND --help`.\n"
    ;
}

int CBLLogCat::run() {
    if (!hasArgs()) {
        cerr << ansiBold()
             << "cbl-log: Couchbase Lite / LiteCore log decoder\n" << ansiReset() 
             << "Missing subcommand.\n"
             << "For a list of subcommands, run " << ansiBold() << "cbl-log help" << ansiReset() << ".\n";
        fail();
    }

    string cmd = nextArg("subcommand or database path");
    if (!processFlag(cmd, {
        {"help",    [&]{helpCommand();}},
        {"logcat",  [&]{logcat();}}
    })) {
        _currentCommand = "";
        failMisuse(format("Unknown subcommand '%s'", cmd.c_str()));
    }

    return 0;
}

void CBLLogCat::logcatUsage() {
    cerr << ansiBold();
    cerr << "cbl-log ";
    cerr << "logcat" << ' ' << ansiItalic();
    cerr << "[FLAGS]" << ' ';
    cerr << "LOG_FILE [...]" << ansiReset() << "\n";
    cerr <<
        "  Converts binary log file(s) to text.\n"
        "  Multiple files are merged together with lines sorted chronologically.\n"
        "  If given a directory, all \".cbllog\" files in that directory are used.\n"
        "    --csv : Output CSV (comma-separated values) format, per RFC4180\n"
        "    --full : Start from time when full logs (all levels) are available\n"
        "    --out FILE_PATH : Write output to FILE_PATH instead of stdout\n"
        "    " << it("LOG_FILE") << " : Path of a log file, or directory of log files\n"
        ;
}

void CBLLogCat::logcat() {
    // Read params:
    bool help = false;
    processFlags({
        {"--csv",  [&] {_csv = true; help = false; }},
        {"--full", [&] {_full = true; help = false; }},
        {"--out",  [&] {_outputFile = nextArg("output file"); help = false; }},
        {"--help", [&] { help = true; }}
        });

    if (help) {
        logcatUsage();
        return;
    }

    MultiLogDecoder decoder;

    unsigned fileCount = 0;
    while (hasArgs()) {
        string logPathStr = nextArg("log file path");
        fixUpPath(logPathStr);

        fs::path logPath{ logPathStr };
        if (fs::is_directory(logPath)) {

            unsigned n = 0;
            for (auto it = fs::directory_iterator(logPath); it != fs::directory_iterator(); it++) {
                if (it->path().extension() == ".cbllog") {
                    if (!decoder.add(it->path().string())) {
                        fail(format("Couldn't open '%s'", it->path().c_str()));
                    }
                    ++n;
                }
            }

            if (n == 0)
                cerr << "No .cbllog files found in " << logPath << "\n";
            fileCount += n;
        }
        else {
            if (!decoder.add(logPath.string()))
                fail(format("Couldn't open '%s'", logPath.c_str()));
            ++fileCount;
        }
    }

    if (fileCount == 0)
        return;
    if (fileCount < 2)
        _full = false;

    if (_outputFile.empty()) {
        if (_csv)
            writeLogCSV(decoder, std::cout);
        else
            writeLog(decoder, std::cout);
    }
    else {
        ofstream out(_outputFile, ofstream::trunc);
        if (!out)
            fail("Couldn't open output file " + _outputFile);
        if (_csv)
            writeLogCSV(decoder, out);
        else
            writeLog(decoder, out);
    }
}

void CBLLogCat::helpCommand() {
    if (!hasArgs()) {
        _showHelp = true; // forces command to show help and return
        string cmd = nextArg("subcommand");
        if (!processFlag(cmd, {
            {"help",    [&]{helpCommand();}},
            {"logcat",  [&]{logcat();}}
        }))
            cerr << format("Unknown subcommand '%s'\n", cmd.c_str());
    } else {
        usage();
    }
}

namespace litecore {
    class error {
    public:
        [[noreturn]] static void assertionFailed(const char* func, const char* file, unsigned line, const char* expr,
            const char* message = nullptr, ...) __printflike(5, 6);
    };

    void error::assertionFailed(const char* fn, const char* file, unsigned line, const char* expr,
        const char* message, ...) {
        string messageStr = "Assertion failed: ";
        if (message) {
            va_list args;
            va_start(args, message);
            messageStr += vformat(message, args);
            va_end(args);
        }
        else {
            messageStr += expr;
        }
        fprintf(stderr, "%s (%s:%u, in %s)", messageStr.c_str(), file, line, fn);
        throw runtime_error(messageStr);
    }
}

// namespace litecore {
//     static void getObjectPathRecur(const LogDomain::ObjectMap& objMap, LogDomain::ObjectMap::const_iterator iter,
//                                    std::stringstream& ss) {
//         // pre-conditions: iter != objMap.end()
//         if ( iter->second.second != 0 ) {
//             auto parentIter = objMap.find(iter->second.second);
//             if ( parentIter == objMap.end() ) {
//                 // the parent object is deleted. We omit the loggingClassName
//                 ss << "/#" << iter->second.second;
//             } else {
//                 getObjectPathRecur(objMap, parentIter, ss);
//             }
//         }
//         ss << "/" << iter->second.first << "#" << iter->first;
//     }

//     std::string LogDomain::getObjectPath(unsigned obj, const ObjectMap& objMap) {
//         auto iter = objMap.find(obj);
//         if ( iter == objMap.end() ) { return ""; }
//         std::stringstream ss;
//         getObjectPathRecur(objMap, iter, ss);
//         return ss.str() + "/";
//     }
// }