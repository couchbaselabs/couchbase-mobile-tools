// 
// V1LogDecoder.cs
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
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using static BinaryEncoding.Binary;

namespace Couchbase.Lite.Binary.Log.Decoders
{
    // Couchbase Lite binary log format v1
    internal sealed class V1LogDecoder : BaseDecoder
    {
        #region Variables

        private readonly Dictionary<uint, TokenString> _objectMap = new Dictionary<uint, TokenString>();
        private readonly Dictionary<uint, TokenString> _tokenMap = new Dictionary<uint, TokenString>();
        private DateTimeOffset _current;

        #endregion

        #region Constructors

        public V1LogDecoder(string filePath, LogDecoder.Options options)
            : base(filePath, options)
        {
            _current = Start;
        }

        #endregion

        #region Private Methods

        private unsafe string FindArgumentsAndFormat(string formatString, IList<ILogArgument> outArgs)
        {
            var sb = new StringBuilder();
            fixed (char* formatPtr = formatString) {
                for (char* c = formatPtr; *c != 0; ++c) {
                    if (*c != '%') {
                        sb.Append(*c);
                    } else {
                        var minus = false;
                        var dotStar = false;
                        ++c;
                        if (*c == '-') {
                            minus = true;
                            ++c;
                        }

                        c += NextPositionNotOf(c, "#0- +'");
                        while (Char.IsDigit(*c)) {
                            c++;
                        }

                        if (*c == '.') {
                            ++c;
                            if (*c == '*') {
                                dotStar = true;
                                ++c;
                            } else {
                                while (Char.IsDigit(*c)) {
                                    ++c;
                                }
                            }
                        }

                        c += NextPositionNotOf(c, "hljtzq");
                        switch (*c) {
                            case 'c':
                            case 'd':
                            case 'i':
                                var negative = _fileIn.ReadByte() > 0;
                                Varint.Read(_fileIn, out ulong tmp);
                                outArgs.Add(new SignedVarintLogArgument(tmp, negative));
                                long param = (long) tmp;
                                if (negative) {
                                    param = -param;
                                }

                                if (*c == 'c') {
                                    sb.Append((char) param);
                                } else {
                                    sb.Append(param);
                                }

                                break;
                            case 'x':
                            case 'X':
                            {
                                Varint.Read(_fileIn, out ulong u);
                                outArgs.Add(new VarintLogArgument(u));
                                sb.AppendFormat("{0:X}", u);
                                break;
                            }
                            case 'u':
                            {
                                Varint.Read(_fileIn, out ulong u);
                                outArgs.Add(new VarintLogArgument(u));
                                sb.Append(u);
                                break;
                            }
                            case 'e':
                            case 'E':
                            case 'f':
                            case 'F':
                            case 'g':
                            case 'G':
                            case 'a':
                            case 'A':
                            {
                                var d = BitConverter.Int64BitsToDouble(LittleEndian.ReadInt64(_fileIn));
                                outArgs.Add(new DoubleLogArgument(d));
                                sb.Append(d);
                                break;
                            }
                            case 's':
                            case '@':
                            {
                                if (minus && !dotStar) {
                                    var nextArg = GetString(_tokenMap, true);
                                    outArgs.Add(new TokenStringLogArgument(nextArg));
                                    sb.Append(nextArg);
                                } else {
                                    Varint.Read(_fileIn, out ulong size);
                                    var buf = stackalloc byte[200];
                                    var nextArg = new ByteSliceLogArgument();
                                    while (size > 0) {
                                        var n = Math.Min((int) size, 200);
                                        _fileIn.Read(new Span<byte>(buf, n));
                                        nextArg.Append(buf, n);
                                        if (minus) {
                                            for (var i = 0; i < n; i++) {
                                                var hex = $"{buf[i]:X2}";
                                                sb.Append(hex);
                                            }
                                        } else {
                                            sb.Append(Encoding.UTF8.GetString(buf, n));
                                        }

                                        size -= (ulong) n;
                                    }

                                    outArgs.Add(nextArg);
                                }

                                break;
                            }
                            case 'p':
                            {
                                sb.Append("0x");
                                if (_pointerSize == 8) {
                                    var nextArg = LittleEndian.ReadUInt64(_fileIn);
                                    sb.Append($"{nextArg:x16}");
                                    outArgs.Add(new LittleEndianLogArgument(nextArg, 8));
                                } else {
                                    var nextArg = LittleEndian.ReadUInt32(_fileIn);
                                    sb.Append($"{nextArg:x8}");
                                    outArgs.Add(new LittleEndianLogArgument(nextArg, 4));
                                }

                                break;
                            }
                            case '%':
                            {
                                sb.Append('%');
                                break;
                            }
                            default:
                                throw new ArgumentException($"Unknown string format type");
                        }
                    }
                }
            }

            return sb.ToString();
        }

        private unsafe int NextPositionNotOf(char* c, string set)
        {
            var realSet = new HashSet<char>(set.ToCharArray());
            char* start = c;
            while (*c != 0 && realSet.Contains(*c)) {
                ++c;
            }

            return (int) (c - start);
        }

        #endregion

        #region Overrides

        public override ValueTask<LogLine> ReadLineAsync()
        {
            Varint.Read(_fileIn, out ulong elapsed);
            _current += TimeSpan.FromTicks((long) elapsed * 10);
            var level = (LogLevel) _fileIn.ReadByte();
            var domain = GetString(_tokenMap, true);
            var obj = GetString(_objectMap, false);
            var formatString = GetString(_tokenMap, false);
            var args = new List<ILogArgument>();
            var message = FindArgumentsAndFormat(formatString.Value, args);

            return new ValueTask<LogLine>(new LogLine(_current, level, domain, obj, formatString, args, message));
        }

        #endregion
    }
}

/* FILE FORMAT:
 
 The file header is:
     Magic number:                  CF B2 AB 1B
     Version number:                [byte]
     Pointer size:                  [byte]              // 04 or 08
     Starting timestamp (time_t):   [varint]
 
 Each logged line contains:
    Microsecs since last line:      [varint]
    Severity level:                 [byte]              // {debug=0, verbose, info, warning, error}
    Domain ID                       [varint]            // Numbered sequentially starting at 0
        name of domain (1st time)   [nul-terminated string]
    Object ID                       [varint]            // Numbered sequentially starting at 1
        obj description (1st time)  [nul-terminated string]
    Format string                   [varint]                 // token ID, same namespace as domains
                                    [nul-terminated string]  // only on 1st appearance of this ID
    Args                            ...
 
 Formats of arguments, by type:
    unsigned integer, any size      [varint]
    signed integer, any size        [sign byte]              // 0 for positive, 1 for negative
                                    [varint]                 // absolute value
    float, double                   [little-endian 8-byte double]
    string (%s, %.*s)               [varint]                 // size
                                    [bytes]
    tokenized string (%-s)          [varint]                 // token ID, same namespace as domains
                                    [nul-terminated string]  // only on 1st appearance of this ID
    pointer (%p)                    [little-endian integer]  // size is given by Pointer Size header
 The next line begins immediately after the final argument.
 There is no file trailer; EOF comes after the last logged line.
*/