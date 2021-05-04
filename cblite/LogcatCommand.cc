//
// LogcatCommand.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "CBLiteCommand.hh"
#include "LogDecoder.hh"
#include "MultiLogDecoder.hh"
#include "FilePath.hh"

using namespace std;
using namespace litecore;


class LogcatCommand : public CBLiteCommand {
public:
    LogcatCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        cerr << ansiBold();
        if (!interactive())
            cerr << "cblite ";
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


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--csv",  [&]{_csv = true;}},
            {"--full", [&]{_full = true;}},
            {"--out",  [&]{_outputFile = nextArg("output file");}},
        });
        MultiLogDecoder decoder;

        unsigned fileCount = 0;
        while (hasArgs()) {
            string logPath = nextArg("log file path");
            fixUpPath(logPath);
            if (FilePath(logPath).existsAsDir()) {
                FilePath dir(logPath, "");
                unsigned n = 0;
                dir.forEachFile([&](const FilePath &item) {
                    if (item.extension() == ".cbllog") {
                        if (!decoder.add(item.path()))
                            fail(format("Couldn't open '%s'", item.path().c_str()));
                        ++n;
                    }
                });
                if (n == 0)
                    cerr << "No .cbllog files found in " << logPath << "\n";
                fileCount += n;
            } else {
                if (!decoder.add(logPath))
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
        } else {
            ofstream out(_outputFile, ofstream::trunc);
            if (!out)
                fail("Couldn't open output file " + _outputFile);
            if (_csv)
                writeLogCSV(decoder, out);
           else
                writeLog(decoder, out);
        }
    }


    void writeLog(MultiLogDecoder &decoder, ostream &out) {
        vector<string> levels = {"***", "", "", "WARNING", "ERROR"};
        if (_outputFile.empty()) {
            levels[3] = ansiBold() + ansiRed() + levels[3] + ansiReset();
            levels[4] = ansiBold() + ansiRed() + levels[4] + ansiReset();
        }

        LogIterator::Timestamp start = decoder.startTime();
        if (_full)
            start = decoder.fullStartTime();

        out << (_full ? "Full log" : "Log") << " begins on " << LogIterator::formatDate(start) << "\n";

        while (decoder.next()) {
            if (decoder.timestamp() < start)
                continue;

            out << ansiDim();
            LogIterator::writeTimestamp(decoder.timestamp(), out);
            out << ansiReset();

            auto level = decoder.level();
            string levelName;
            if (level >= 0 && level < levels.size())
                levelName = levels[level];
            LogIterator::writeHeader(levelName, decoder.domain(), out);
            decoder.decodeMessageTo(out);
            out << '\n';
        }
    }


    void writeLogCSV(MultiLogDecoder &decoder, ostream &out) {
        // CSV format as per https://tools.ietf.org/html/rfc4180

        vector<string> levels = {"debug", "verbose", "info", "warning", "error"};

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

private:
    bool                    _full {false};
    bool                    _csv {false};
    std::string             _outputFile;
};


CBLiteCommand* newLogcatCommand(CBLiteTool &parent) {
    return new LogcatCommand(parent);
}
