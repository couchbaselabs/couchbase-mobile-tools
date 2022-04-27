// 
// BaseEncoder.cs
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
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log.Decoders;

using static BinaryEncoding.Binary;

namespace Couchbase.Lite.Binary.Log.Encoders
{
    internal abstract class BaseEncoder : ILogEncoder
    {
        #region Properties

        protected FileStream FileOut { get; }

        #endregion

        #region Constructors

        protected BaseEncoder(FileStream fout, DateTimeOffset start, byte version)
        {
            // See various LogDecoder files for encoding
            FileOut = fout;
            FileOut.Write(LogDecoder.LogMagic);
            FileOut.WriteByte(version);
            FileOut.WriteByte(4); // Guess now, correct later if a larger one is found
            Varint.Write(FileOut, (ulong) start.ToUniversalTime().ToUnixTimeSeconds());
        }

        #endregion

        #region Protected Methods

        protected virtual void DisposeInternal()
        {
        }

        protected void SetPointerSize(int size)
        {
            var pos = FileOut.Position;
            FileOut.Seek(5, SeekOrigin.Begin);
            FileOut.WriteByte((byte) size);
            FileOut.Seek(pos, SeekOrigin.Begin);
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            DisposeInternal();
            FileOut.Dispose();
        }

        #endregion

        #region ILogEncoder

        public abstract ValueTask WriteAsync(LogLine line);

        #endregion
    }
}