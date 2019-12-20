// 
// ILogEncoder.cs
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
using System.Threading.Tasks;

namespace Couchbase.Lite.Binary.Log.Encoders
{
    /// <summary>
    /// An interface that can encode LogLines in the proprietary
    /// Couchbase Lite binary format
    /// </summary>
    public interface ILogEncoder : IDisposable
    {
        #region Public Methods

        /// <summary>
        /// Writes a line to the underlying log file
        /// </summary>
        /// <param name="line">The line to write</param>
        /// <returns>An awaitable task</returns>
        ValueTask WriteAsync(LogLine line);

        #endregion
    }
}