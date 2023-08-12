// 
// TextLogLineEnumerator.cs
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
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;

namespace LogMergeTool.Inputs
{
    sealed partial class LogLineEnumerator
    {
        // If the log is plaintext, this class will be used to enumerate
        private class TextLogLineEnumerator : IAsyncEnumerator<LogLine>
        {
            #region Variables

            private readonly StreamReader _fileRead;
            private readonly LogLevel _level;
            private readonly DateTimeOffset _startTime;

            private TimeSpan _daysOffset = TimeSpan.Zero;
            private TimeSpan _lastOffset = TimeSpan.Zero;

            #endregion

            #region Properties

            public LogLine Current { get; private set; }

            #endregion

            #region Constructors

            public TextLogLineEnumerator(string logFilePath, DateTimeOffset startTime, LogLevel level)
            {
                var stream = File.Open(logFilePath, FileMode.Open, FileAccess.Read, FileShare.Read);
                _fileRead = new StreamReader(stream);
                _startTime = startTime;
                _level = level;
            }

            #endregion

            #region IAsyncDisposable

            public ValueTask DisposeAsync()
            {
                _fileRead.Dispose();
                return new ValueTask(Task.CompletedTask);
            }

            #endregion

            #region IAsyncEnumerator<LogLine>

            public async ValueTask<bool> MoveNextAsync()
            {
                if (_fileRead.EndOfStream) {
                    return false;
                }

                var rawLine = await _fileRead.ReadLineAsync().ConfigureAwait(false);
                Debug.Assert(rawLine != null, nameof(rawLine) + " != null");

                if (rawLine.StartsWith("----")) {
                    // The first or second line of the log, no timestamp
                    var startTime = Advanced.AsBoolean(ConfigurationKeys.Format.Utc) ? _startTime.UtcDateTime : _startTime.LocalDateTime;
                    Current = new LogLine(startTime, _level, rawLine);
                } else {
                    var timestampEnd = rawLine.IndexOf('|');
                    var timestampStr = rawLine.Substring(0, timestampEnd);
                    var utcInput = timestampStr.EndsWith("Z");
                    var utcOutput = Advanced.AsBoolean(ConfigurationKeys.Format.Utc);
                    var endOffset = 0;
                    if(utcInput) {
                        endOffset = 1;
                    }

                    var timestamp = timestampEnd == -1 ? _lastOffset 
                        : TimeSpan.ParseExact(rawLine.Substring(0, timestampEnd - endOffset), "c",
                            CultureInfo.InvariantCulture);

                    DateTime finalTime;
                    if(utcInput) {
                        finalTime = _startTime.UtcDateTime.Date + _daysOffset + timestamp;
                        if(!utcOutput) {
                            finalTime = finalTime.ToLocalTime();
                        }
                    } else {
                        finalTime = _startTime.LocalDateTime.Date + _daysOffset + timestamp;
                        if(utcOutput) {
                            finalTime = finalTime.ToUniversalTime();
                        }
                    }

                    if (timestamp < _lastOffset) {
                        // 23:59 -> 00:00 means the next day
                        _daysOffset += TimeSpan.FromDays(1);
                    }

                    Current = new LogLine(finalTime, _level, rawLine.Substring(timestampEnd + 1));
                    _lastOffset = timestamp;
                }

                return true;
            }

            #endregion
        }
    }
}