// 
// LogLine.cs
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

namespace Couchbase.Lite.Binary.Log
{
    /// <summary>
    /// A class holding information about a log entry in
    /// a Couchbase Lite log
    /// </summary>
    public sealed class LogLine
    {
        #region Properties

        public IReadOnlyList<ILogArgument> Arguments { get; }

        /// <summary>
        /// Gets the domain of the log (Database, Sync, etc)
        /// </summary>
        public TokenString Domain { get; }

        public string FormattedMessage { get; }

        /// <summary>
        /// Gets the level of the log line
        /// </summary>
        public LogLevel Level { get; }

        /// <summary>
        /// Gets the message that was written from the library
        /// </summary>
        public TokenString MessageFormat { get; }

        /// <summary>
        /// Gets the object that the message was associated with
        /// </summary>
        public TokenString Object { get; }

        /// <summary>
        /// Gets the time that the message was written
        /// </summary>
        public DateTimeOffset Time { get; }

        #endregion

        #region Constructors

        public LogLine(DateTimeOffset time, LogLevel level, string message)
        {
            Time = time;
            Level = level;
            FormattedMessage = message;
        }

        public LogLine(DateTimeOffset time, LogLevel level, TokenString domain,
            TokenString obj, TokenString messageFormat, IReadOnlyList<ILogArgument> arguments,
            string formattedMessage)
        {
            Time = time;
            Level = level;
            Domain = domain;
            Object = obj;
            MessageFormat = messageFormat;
            Arguments = arguments;
            FormattedMessage = formattedMessage;
        }

        #endregion

        #region Overrides

        public override bool Equals(object obj)
        {
            if (!(obj is LogLine other)) {
                return false;
            }

            return Time == other.Time && FormattedMessage == other.FormattedMessage;
        }

        public override int GetHashCode()
        {
            unchecked {
                return (FormattedMessage.GetHashCode() * 397) ^ Time.GetHashCode();
            }
        }

        #endregion
    }
}