// 
// BinaryLogLineEnumerator.cs
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

using System.Collections.Generic;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;
using Couchbase.Lite.Binary.Log.Decoders;

namespace LogMergeTool.Inputs
{
    sealed partial class LogLineEnumerator
    {
        // If the log is binary, this class will be used to enumerate
        private sealed class BinaryLogLineEnumerator : IAsyncEnumerator<LogLine>
        {
            #region Variables

            private readonly ILogDecoder _decoder;

            #endregion

            #region Properties

            public LogLine Current { get; private set; }

            #endregion

            #region Constructors

            public BinaryLogLineEnumerator(string logFilePath)
            {
                var useUtc = Advanced.AsBoolean(ConfigurationKeys.Format.Utc);
                _decoder = LogDecoder.Create(logFilePath, new LogDecoder.Options
                {
                    UseUtc = useUtc
                });
            }

            #endregion

            #region IAsyncDisposable

            public ValueTask DisposeAsync()
            {
                return _decoder.DisposeAsync();
            }

            #endregion

            #region IAsyncEnumerator<LogLine>

            public async ValueTask<bool> MoveNextAsync()
            {
                if (_decoder.Finished) {
                    return false;
                }

                Current = await _decoder.ReadLineAsync().ConfigureAwait(false);
                return true;
            }

            #endregion
        }
    }
}