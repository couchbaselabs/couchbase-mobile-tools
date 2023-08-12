// 
// Program.cs
// 
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;
using Couchbase.Lite.Binary.Log.Decoders;
using Couchbase.Lite.Binary.Log.Encoders;

using LogMergeTool.Inputs;
using LogMergeTool.Outputs;

using McMaster.Extensions.CommandLineUtils;

using Utility.CommandLine;

namespace LogMergeTool
{
    internal enum LogFileType
    {
        NotALog,
        Binary,
        Text
    }

    [Command(Name = "LogMergeTool",
        Description =
            "Merges all log files in a given directory into inclusive level logs.  For more advanced tuning see the README.md file")]
    [HelpOption("-?")]
    partial class Program
    {

        #region Constants

        private static readonly LogLineCollection SortedLines = new LogLineCollection();

        private static readonly HashSet<string> SeenVersions = new HashSet<string>();

        #endregion

        #region Variables

        private static DateTimeOffset EarliestStart = DateTimeOffset.MaxValue;

        #endregion

        #region Public Methods

        public static void Error(string msg)
        {
            Console.ForegroundColor = ConsoleColor.DarkRed;
            Console.WriteLine(msg);
            Console.ResetColor();
        }

        public static void Warn(string msg)
        {
            Console.ForegroundColor = ConsoleColor.DarkYellow;
            Console.WriteLine(msg);
            Console.ResetColor();
        }

        #endregion

        #region Private Methods

        static async Task<int> Main(string[] args)
        {
            try {
                return await CommandLineApplication.ExecuteAsync<Program>(args);
            } catch (IOException e) {
                if (unchecked(e.HResult == (int) 0x80070050)) {
                    Error("One of the files to be written already exists, use --force to overwrite");
                    Error(e.Message);
                    return -1;
                }

                throw;
            } finally {
                SortedLines.Dispose();
            }
        }

        private async ValueTask<int> AddToCollection(string logFile, LogFileType logType)
        {
            var logLines = new LogLineEnumerator(logFile, logType);
            var first = true;
            var retVal = -1;
            await foreach (var line in logLines) {
                if (first) {
                    if (line.Time < EarliestStart) {
                        EarliestStart = line.Time;
                    }

                    if (!line.FormattedMessage.Contains("serialNo")) {
                        SeenVersions.Add(line.FormattedMessage);
                        first = false;
                        continue;
                    } else {
                        var start = line.FormattedMessage.IndexOf("serialNo=") + "serialNo=".Count();
                        var chars = new string(line.FormattedMessage.Skip(start).TakeWhile(x => x >= '0' && x <= '9').ToArray());
                        retVal = Int32.Parse(chars);
                    }
                }

                SortedLines.Add(line);
            }

            return retVal;
        }

        private void Progress(ProgressBar bar, int value, string label)
        {
            bar.Value = value;
            var left = Console.CursorLeft;
            var top = Console.CursorTop;
            Console.SetCursorPosition(0, 0);
            Console.Write(new string(' ', Console.BufferWidth));
            Console.SetCursorPosition(0, 0);
            Console.WriteLine(label);
            Console.Write(bar);
            Console.SetCursorPosition(left, top);
        }

        private async Task<int> OnExecute()
        {
            Console.Clear();

            // Get ahead of time so as not to pick up files created during
            // the process
            var filesAvailable = Directory.EnumerateFiles(LogPath)
                .Where(x => x != Path.GetFullPath(Output))
                .ToArray();

            var serialNumbers = new Dictionary<string, int>();
            var filesUsed = new List<string>();
            var steps = filesAvailable.Length + 2;
            var currentStep = 0;
            var bar = new ProgressBar(maximum: steps);
            Console.CursorTop = 3;

            foreach (var logFile in filesAvailable) {
                Progress(bar, currentStep, $"Reading {Path.GetFileName(logFile)}...");
                var logType = await TypeOfLog(logFile);
                if (logType == LogFileType.NotALog) {
                    Warn($"Skipping non-log file {logFile}");
                    ++currentStep;
                    continue;
                }

                filesUsed.Add(Path.GetFileName(logFile));
                var serialNo = await AddToCollection(logFile, logType);
                serialNumbers[Path.GetFileName(logFile)] = serialNo;
                ++currentStep;
            }

            Progress(bar, currentStep++, "Normalizing Tokens...");
            Progress(bar, currentStep++, "Writing Output...");
            using var logOutput = CreateOutput();
            await logOutput.WriteAsync("BEGIN METADATA");
            await logOutput.WriteAsync($"{Environment.NewLine}FILES USED");
            foreach (var file in filesUsed) {
                var serialNo = serialNumbers[file];
                if (serialNo != -1) {
                    await logOutput.WriteAsync($"{file} (serial number {serialNo})");
                } else {
                    await logOutput.WriteAsync($"{file}");
                }
            }

            await logOutput.WriteAsync($"{Environment.NewLine}VERSIONS INVOLVED");
            foreach (var version in SeenVersions) {
                await logOutput.WriteAsync(version);
            }

            await logOutput.WriteAsync($"{Environment.NewLine}START DATE (MM/dd/yy)");
            await logOutput.WriteAsync($"{EarliestStart:dddd, MM/dd/yy}");
            await logOutput.WriteAsync($"{Environment.NewLine}END METADATA{Environment.NewLine}");

            foreach (var line in SortedLines) {
                await logOutput.WriteAsync(line);
            }

            Progress(bar, currentStep, "Complete!");

            return 0;
        }

        private ILogOutput CreateOutput()
        {
            switch (OutputType) {
                case OutputFormat.SingleFile:
                    return new FileLogOutput(Output, Force);
                case OutputFormat.SyncGateway:
                    return new SyncGatewayOutput(Output, Force);
                case OutputFormat.LogLady:
                    return new LogLadyOutput(Output, Force, EarliestStart);
                default:
                    throw new NotSupportedException($"Unknown output type {OutputType}");
            }
        }

        private async Task<LogFileType> TypeOfLog(string filePath)
        {
            const int sampleSize = 18;

            FileStream fin = null;
            try {
                fin = File.OpenRead(filePath);
                if (fin.Length < sampleSize) {
                    return LogFileType.NotALog;
                }

                var sample = new byte[sampleSize];
                await fin.ReadAsync(sample);
                for (int i = 0; i < 4; i++) {
                    if (sample[i] != LogDecoder.LogMagic[i]) {
                        break;
                    }

                    if (i == 3) {
                        return LogFileType.Binary;
                    }
                }

                string sampleString = Encoding.ASCII.GetString(sample);
                return sampleString == "---- CouchbaseLite" ? LogFileType.Text : LogFileType.NotALog;
            } catch (IOException) {
                return LogFileType.NotALog;
            } finally {
                if (fin != null) {
                    await fin.DisposeAsync();
                }
            }
        }

        #endregion
    }
}