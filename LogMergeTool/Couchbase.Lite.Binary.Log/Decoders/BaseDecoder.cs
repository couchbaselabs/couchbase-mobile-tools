// 
// BaseDecoder.cs
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

namespace Couchbase.Lite.Binary.Log.Decoders
{
    /// <summary>
    /// A base class containing common functionality for any decoder that will
    /// decode Couchbase binary logs
    /// </summary>
    internal abstract class BaseDecoder : ILogDecoder
    {
        #region Variables

        /// <summary>
        /// The stream to the file being decoded
        /// </summary>
        protected readonly FileStream _fileIn;

        /// <summary>
        /// The size of the pointers written to the binary file as indicated
        /// by the header
        /// </summary>
        protected readonly int _pointerSize;

        #endregion

        #region Properties

        /// <summary>
        /// Gets the start date and time as parsed out of the filename of the log
        /// </summary>
        public DateTimeOffset Start { get; }

        /// <inheritdoc />
        public bool Finished => _fileIn.Position == _fileIn.Length;

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="filePath">The path of the file to read</param>
        /// <param name="options">The options to use when opening the file</param>
        protected BaseDecoder(string filePath, LogDecoder.Options options)
        {
            _fileIn = File.Open(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            _fileIn.Seek(5, SeekOrigin.Begin);
            _pointerSize = _fileIn.ReadByte();
            Varint.Read(_fileIn, out ulong startMs);
            Start = DateTimeOffset.FromUnixTimeSeconds((long) startMs);
        }

        #endregion

        #region Protected Methods

        /// <summary>
        /// Internal disposal logic.  Override in subclasses as necessary
        /// </summary>
        /// <returns>An awaitable task</returns>
        protected virtual ValueTask DisposeAsyncInternal()
        {
            return new ValueTask(Task.CompletedTask);
        }

        /// <summary>
        /// Gets a binary token string from the stream of data.
        /// </summary>
        /// <param name="map">The backing store for seen strings</param>
        /// <param name="allowZero">Whether or not to allow zero as a key (if not,
        /// 0 will always be an empty token string)</param>
        /// <returns>The token string (with id and value)</returns>
        protected TokenString GetString(IDictionary<uint, TokenString> map, bool allowZero)
        {
            Varint.Read(_fileIn, out uint id);
            if (id == 0 && !allowZero) {
                return new TokenString(0, null);
            }

            if (map.TryGetValue(id, out var storedValue)) {
                return storedValue;
            }

            storedValue = new TokenString(id, ReadString());
            map[id] = storedValue;
            return storedValue;
        }

        /// <summary>
        /// Reads a string from the stream (UTF-8 encoded)
        /// </summary>
        /// <param name="length">If the default, -1, the string must be
        /// null encoded.  Otherwise, the length (in *bytes*) is the amount
        /// that will be read from the stream</param>
        /// <returns></returns>
        protected string ReadString(int length = -1)
        {
            var isTerminated = length == -1;
            if (isTerminated) {
                length = 0;
                while (_fileIn.ReadByte() != 0) {
                    length++;
                }

                _fileIn.Seek(-length - 1, SeekOrigin.Current);
            }

            var stringBytes = new byte[length];
            _fileIn.Read(stringBytes);
            if (isTerminated) {
                _fileIn.Seek(1, SeekOrigin.Current);
            }

            return Encoding.UTF8.GetString(stringBytes);
        }

        #endregion

        #region IAsyncDisposable

        /// <inheritdoc />
        public async ValueTask DisposeAsync()
        {
            await DisposeAsyncInternal().ConfigureAwait(false);
            await _fileIn.DisposeAsync().ConfigureAwait(false);
        }

        #endregion

        #region ILogDecoder

        /// <inheritdoc />
        public abstract ValueTask<LogLine> ReadLineAsync();

        #endregion
    }
}