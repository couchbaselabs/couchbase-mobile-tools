// 
// LogLineEnumerator.cs
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
using System.Threading;

using Couchbase.Lite.Binary.Log;

namespace LogMergeTool.Inputs
{
    // 
    sealed partial class LogLineEnumerator : IAsyncEnumerable<LogLine>
    {
        #region Variables

        private readonly string _logFilePath;
        private readonly LogFileType _type;
        private readonly DateTimeOffset? _start;

        #endregion

        #region Properties

        public string LinePrefix { get; }

        internal LogLevel LogLevel { get; }

        #endregion

        #region Constructors

        public LogLineEnumerator(string logFilePath, LogFileType type)
        {
            _logFilePath = logFilePath;
            _type = type;
            if (type == LogFileType.Text) {
                var filenameParts = Path.GetFileNameWithoutExtension(logFilePath).Split('_');
                if (filenameParts.Length != 3) {
                    Program.Warn($"Skipping log file {logFilePath}, which has no timestamp information");
                    return;
                }

                if (!Int64.TryParse(filenameParts[2], out var startMs)) {
                    Program.Warn($"Skipping log file with improper timestamp {filenameParts[2]} in {logFilePath}");
                    return;
                }

                _start = DateTimeOffset.FromUnixTimeMilliseconds(startMs);
                if (!Advanced.AsBoolean(ConfigurationKeys.Format.Utc)) {
                    _start = _start.Value.ToLocalTime();
                }

                switch (filenameParts[1]) {
                    case "error":
                        LogLevel = LogLevel.Error;
                        break;
                    case "warning":
                        LogLevel = LogLevel.Warn;
                        break;
                    case "info":
                        LogLevel = LogLevel.Info;
                        break;
                    case "verbose":
                        LogLevel = LogLevel.Verbose;
                        break;
                    case "debug":
                        LogLevel = LogLevel.Debug;
                        break;
                }

                LinePrefix = filenameParts[1].Substring(0, 1).ToUpperInvariant();
            }
        }

        #endregion

        #region IAsyncEnumerable<LogLine>

        public IAsyncEnumerator<LogLine> GetAsyncEnumerator(CancellationToken cancelToken)
        {
            if (_type == LogFileType.Binary) {
                return new BinaryLogLineEnumerator(_logFilePath);
            }

            if (_start == null) {
                return new EmptyLogLineEnumerator();
            }

            return new TextLogLineEnumerator(_logFilePath, _start.Value, LogLevel);
        }

        #endregion
    }
}