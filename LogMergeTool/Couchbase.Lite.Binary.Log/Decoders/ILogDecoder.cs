// 
// ILogDecoder.cs
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

namespace Couchbase.Lite.Binary.Log.Decoders
{
    /// <summary>
    /// An interface describing a decoder capable of line-by-line decoding
    /// of a Couchbase Lite binary log
    /// </summary>
    public interface ILogDecoder : IAsyncDisposable
    {
        #region Properties

        /// <summary>
        /// Gets whether or not the decoder is finished (i.e. has no more
        /// data to read) 
        /// </summary>
        bool Finished { get; }

        #endregion

        #region Public Methods

        /// <summary>
        /// Asynchronously reads a line from the log file
        /// </summary>
        /// <returns>The next line of the log</returns>
        ValueTask<LogLine> ReadLineAsync();

        #endregion
    }
}