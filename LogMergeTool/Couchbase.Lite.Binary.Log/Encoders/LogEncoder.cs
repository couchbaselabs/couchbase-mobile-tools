// 
// LogEncoder.cs
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
using System.IO;

namespace Couchbase.Lite.Binary.Log.Encoders
{
    /// <summary>
    /// A factory for creating <see cref="ILogEncoder"/> instances
    /// </summary>
    public static class LogEncoder
    {
        #region Public Methods

        /// <summary>
        /// Creates a log encoder which will log messages starting at
        /// the given date
        /// </summary>
        /// <param name="version">The version of the log encoder to create</param>
        /// <param name="fout">The file stream to write to</param>
        /// <param name="start">The date of the first log message</param>
        /// <returns>The created log encoder</returns>
        public static ILogEncoder CreateEncoder(int version, FileStream fout, DateTimeOffset start)
        {
            switch (version) {
                case 1:
                    return new V1LogEncoder(fout, start);
                default:
                    throw new NotSupportedException($"Unsupported log version {version}");
            }
        }

        #endregion
    }
}