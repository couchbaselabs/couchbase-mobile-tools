// 
// V1LogEncoder.cs
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
using System.Text;
using System.Threading.Tasks;

using static BinaryEncoding.Binary;

namespace Couchbase.Lite.Binary.Log.Encoders
{
    // See V1LogDecoder for format
    internal sealed class V1LogEncoder : BaseEncoder
    {
        #region Variables

        private readonly HashSet<uint> _writtenObjects = new HashSet<uint>();
        private readonly HashSet<uint> _writtenTokens = new HashSet<uint>();
        private DateTimeOffset _current;
        private bool _pointerSizeKnown;

        #endregion

        #region Constructors

        public V1LogEncoder(FileStream fout, DateTimeOffset start) : base(fout, start, 1)
        {
            _current = start;
        }

        #endregion

        #region Private Methods

        private async ValueTask WriteObjectString(TokenString str)
        {
            await Varint.WriteAsync(FileOut, str.Id).ConfigureAwait(false);
            if (str.Id > 0 && !_writtenObjects.Contains(str.Id)) {
                _writtenObjects.Add(str.Id);
                await FileOut.WriteAsync(Encoding.UTF8.GetBytes(str.Value)).ConfigureAwait(false);
                FileOut.WriteByte(0);
            }
        }

        private async ValueTask WriteTokenString(TokenString str)
        {
            await Varint.WriteAsync(FileOut, str.Id).ConfigureAwait(false);
            if (!_writtenTokens.Contains(str.Id)) {
                _writtenTokens.Add(str.Id);
                await FileOut.WriteAsync(Encoding.UTF8.GetBytes(str.Value)).ConfigureAwait(false);
                FileOut.WriteByte(0);
            }
        }

        #endregion

        #region Overrides

        public override async ValueTask WriteAsync(LogLine line)
        {
            if (line.Domain == null || line.MessageFormat == null) {
                throw new ArgumentException("Log line does not have enough information for binary encoding");
            }

            var elapsed = line.Time - _current;
            _current = line.Time;
            var microsecs = elapsed.Ticks / 10;
            await Varint.WriteAsync(FileOut, (ulong) microsecs).ConfigureAwait(false);
            FileOut.WriteByte((byte) line.Level);
            await WriteTokenString(line.Domain).ConfigureAwait(false);
            await WriteObjectString(line.Object).ConfigureAwait(false);
            await WriteTokenString(line.MessageFormat).ConfigureAwait(false);
            if (line.Arguments != null) {
                foreach (var arg in line.Arguments) {
                    // HACK
                    if (!_pointerSizeKnown && arg is LittleEndianLogArgument e) {
                        SetPointerSize(e.Size);
                        _pointerSizeKnown = true;
                    }

                    await arg.WriteAsync(FileOut).ConfigureAwait(false);
                }
            }
        }

        #endregion
    }
}