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
#include "MultiLogDecoder.hh"
#include "FilePath.hh"
#include <iomanip>
#include <unordered_set>

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
        "  Displays Couchbase Lite log files in readable form. Decodes binary logs.\n"
        "  Multiple files are merged together with lines sorted chronologically.\n"
        "  If given a directory, all \".cbllog\" files in that directory are read.\n"
        "    --csv           : Output CSV (comma-separated values) format, per RFC4180\n"
        "    --full          : Start from time when full logs (all levels) are available\n"
        "    --highlight STR : Highlight occurrences of STR in messages (not in CSV)\n"
        "    --level DOM:LVL : Set min level LVL (debug, verbose, info, warn, error) to show for domain DOM\n"
        "    --level LEVEL   : Set min level to show for unspecified domains\n"
        "    --lineno        : Show line numbers (not in CSV)\n"
        "    --out FILE_PATH : Write output to FILE_PATH instead of stdout\n"
        "    --time MODE     : Display times as full date-time ('full') or as seconds since start ('rel')\n"
        "    " << it("LOG_FILE") << " : Path of a log file, or directory of log files\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--csv",       [&]{_csv = true;}},
            {"--filter",    [&]{_filter = nextArg("filter string");}},
            {"--full",      [&]{_full = true;}},
            {"--out",       [&]{_outputFile = nextArg("output file");}},
            {"--offset",    [&]{offsetFlag();}},
            {"--limit",     [&]{limitFlag();}},
            {"--lineno",    [&]{_lineNumbers = true;}},
            {"--level",     [&]{setMinLevelArg(nextArg("level"));}},
            {"--time",      [&]{_relativeTime = (nextArg("time mode") == "rel");}},
            {"--highlight", [&]{_highlight = nextArg("highlight string");}},
        });

        unsigned fileCount = 0;
        while (hasArgs()) {
            string logPath = nextArg("log file path");
            fixUpPath(logPath);
            if (FilePath(logPath).existsAsDir()) {
                FilePath dir(logPath, "");
                unsigned n = 0;
                dir.forEachFile([&](const FilePath &item) {
                    if (item.extension() == ".cbllog") {
                        addLogFile(item.path());
                        ++n;
                    }
                });
                if (n == 0)
                    cerr << "No .cbllog files found in " << logPath << "\n";
                fileCount += n;
            } else {
                addLogFile(logPath);
                ++fileCount;
            }
        }

        if (fileCount == 0)
            return;
        if (fileCount < 2)
            _full = false;

        if (_outputFile.empty()) {
            writeLog(cout);
        } else {
            ofstream out(_outputFile, ofstream::trunc);
            if (!out)
                fail("Couldn't open output file " + _outputFile);
            writeLog(out);
        }
    }


    // Parses a comma-delimited list of level settings of the form `Domain:level` or `level`.
    void setMinLevelArg(string arg) {
        split(arg, ",", [&](string_view item) {
            string domain;
            if (auto pos = item.find(':'); pos != string::npos) {
                domain = item.substr(0, pos);
                item = item.substr(pos + 1);
            }
            string levelName = lowercase(string(item));
            for (int level = 0; level < size(kLevelNames); ++level) {
                if (kLevelNames[level].starts_with(levelName)) {
                    if (domain.empty())
                        _minLevel = C4LogLevel(level);
                    else
                        _minLevelByDomain[domain] = C4LogLevel(level);
                    return;
                }
            }
            fail("'" + levelName + "' is not a valid log level; use debug, verbose, info, warning or error");
        });
    }


    void addLogFile(string const& path) {
        try {
            if (!_decoder.add(path))
                fail(stringprintf("Couldn't open '%s'", path.c_str()));
        } catch (exception const& x) {
            fail(stringprintf("Couldn't open '%s': %s", path.c_str(), x.what()));
        }
    }


    void writeLog(ostream &out) {
        array<string, 5> levels = {"d", "v", "I", "W", "E"};
        if (_outputFile.empty()) {
            levels[0] = ansiDim() + levels[0];
            levels[1] = ansiDim() + levels[1];
            levels[3] = ansiBold() + ansiRed() + levels[3] + ansiReset();
            levels[4] = ansiBold() + ansiRed() + levels[4] + ansiReset();
        }

        _startTime = _decoder.startTime();
        if (_full)
            _startTime = std::max(_startTime,  _decoder.fullStartTime());

        if (_csv)
            out << "Time,Level,Domain,Object,Message\r\n";

        string highlightedStr = ansi("43") + _highlight + ansi("49");  // yellow bg
        if (highlightedStr == _highlight)
            highlightedStr = "▶︎▶︎" + _highlight + "◀︎◀︎";  // fallback without ANSI color

        while (_decoder.next()) {
            ++_lineNo;
            if (_decoder.timestamp() < _startTime || !shouldShowMessage())
                continue;
            if (_limit >= 0 && _limit-- == 0)
                break;

            if (_csv) {
                // CSV format as per https://tools.ietf.org/html/rfc4180
                writeTimestamp(_decoder.timestamp(), out);
                out << ',';
                if (auto level = _decoder.level(); level >= 0 && level < kLevelNames.size())
                    out << kLevelNames[level];
                else
                    out << int(level);
                out << ',' << _decoder.domain() << ',';
                if (auto desc = _decoder.objectDescription())
                    out << quoted(*desc);
                out << "," << quoted(_decoder.readMessage()) << "\r\n";

            } else {
                // Line number:
                out << ansiDim();
                if (_lineNumbers)
                    out << setw(5) << right << _lineNo << ' ';

                // Timestamp:
                writeTimestamp(_decoder.timestamp(), out);
                out << ansiReset() << ' ';

                // Level:
                string levelName;
                if (auto level = _decoder.level(); level >= 0 && level < levels.size())
                    levelName = levels[level];
                out << levelName << ' ' << setw(6) << left << _decoder.domain() << "⏐ ";

                // Object ID:
                if (auto objID = _decoder.objectID()) {
                    string objName;
                    if (auto i = _objects.find(objID); i == _objects.end()) {
                        // First time we've seen this object ID:
                        auto path = split(*_decoder.objectDescription(), "/");
                        ranges::reverse(path);
                        objName = join(path, " ←");
                        _objects.insert({objID, string(path[0])});
                    } else {
                        objName = i->second;
                    }

                    out << ansiItalic() << "⟦";
                    if (objName == _highlight)
                        out << highlightedStr;
                    else
                        out << objName;
                    out << "⟧ " << ansiNoItalic();
                }

                // Log message:
                if (_highlight.empty()) {
                    _decoder.decodeMessageTo(out);
                } else {
                    string message = _decoder.readMessage();
                    string_view messagev = message;
                    size_t lastPos = 0, pos;
                    while ((pos = messagev.find(_highlight, lastPos)) != string::npos) {
                        out << messagev.substr(lastPos, pos - lastPos);
                        out << highlightedStr;
                        lastPos = pos + _highlight.size();
                    }
                    out << messagev.substr(lastPos);
                }
                out << ansiReset() << '\n';
            }
        }
    }


    bool shouldShowMessage() {
        if (_offset > 0) {
            --_offset;
            return false;
        }
        auto level = C4LogLevel(_decoder.level());
        if (auto i = _minLevelByDomain.find(_decoder.domain()); i != _minLevelByDomain.end()) {
            if (level < i->second)
                return false;
        } else {
            if (level < _minLevel)
                return false;
        }
        if (!_filter.empty()) {
            if (_decoder.readMessage().find(_filter) != string::npos)
                return true;
            if (auto obj = _decoder.objectDescription(); obj && obj->find(_filter) != string::npos)
                return true;
            return false;
        }
        return true;
    }


    void writeTimestamp(LogDecoder::Timestamp t, ostream& out) {
        if (_relativeTime) {
            t.secs -= _startTime.secs;
            if (int micro = int(t.microsecs) - int(_startTime.microsecs); micro >= 0) {
                t.microsecs = micro;
            } else {
                t.microsecs = 1000000 + micro;
                --t.secs;
            }
            out << stringprintf("%02ld:%02ld:%02ld.%06u",
                t.secs / 3600, (t.secs % 3600) / 60, t.secs % 60, t.microsecs);
        } else {
            LogIterator::writeISO8601DateTime(t, out);
        }
    }


private:
    static constexpr array<string_view,6> kLevelNames = {
        "debug", "verbose", "info", "warning", "error", "none"
    };

    bool                             _full = false;
    bool                             _csv = false;
    bool                             _relativeTime = false;
    bool                             _lineNumbers = false;
    C4LogLevel                       _minLevel = kC4LogDebug;
    unordered_map<string,C4LogLevel> _minLevelByDomain;
    string                           _filter;
    string                           _highlight;

    string                           _outputFile;
    LogDecoder::Timestamp            _startTime;
    MultiLogDecoder                  _decoder;
    size_t                           _lineNo = 0;
    unordered_map<uint64_t,string>   _objects;
};


CBLiteCommand* newLogcatCommand(CBLiteTool &parent) {
    return new LogcatCommand(parent);
}
