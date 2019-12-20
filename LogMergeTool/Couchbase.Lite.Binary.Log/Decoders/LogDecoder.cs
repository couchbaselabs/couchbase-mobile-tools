// 
// LogDecoder.cs
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

namespace Couchbase.Lite.Binary.Log.Decoders
{
    /// <summary>
    /// A factory for creating <see cref="ILogDecoder"/> instances
    /// </summary>
    public static class LogDecoder
    {
        #region Constants

        public static readonly byte[] LogMagic = {0xCF, 0xB2, 0xAB, 0x1B};

        #endregion

        #region Public Methods

        /// <summary>
        /// Creates the appropriate log decoder for the file at the given path
        /// </summary>
        /// <param name="path">The path to the file to open</param>
        /// <param name="options">The options to use when opening</param>
        /// <returns>The decoder that can decode the file</returns>
        /// <exception cref="FileNotFoundException">The file at path does not exist</exception>
        /// <exception cref="InvalidDataException">The file at the path is not a Couchbase Lite binary log</exception>
        /// <exception cref="NotSupportedException">The version of the binary log is not supported</exception>
        public static ILogDecoder Create(string path, Options options)
        {
            if (!File.Exists(path)) {
                throw new FileNotFoundException("Unable to open log file", path);
            }

            using var stream = File.OpenRead(path);
            Span<byte> magic = stackalloc byte[4];
            stream.Read(magic);
            if (!magic.SequenceEqual(LogMagic)) {
                throw new InvalidDataException("Invalid log file passed (magic number incorrect)!");
            }

            var version = stream.ReadByte();
            switch (version) {
                case 1:
                    return new V1LogDecoder(path, options);
                default:
                    throw new NotSupportedException($"Unsupported log version {version}");
            }
        }

        #endregion

        /// <summary>
        /// The options for opening and decoding a binary log
        /// </summary>
        public ref struct Options
        {
            #region Variables

            /// <summary>
            /// If <c>true</c> then print the timestamps in UTC time for
            /// text output formats
            /// </summary>
            public bool UseUtc;

            #endregion
        }
    }
}