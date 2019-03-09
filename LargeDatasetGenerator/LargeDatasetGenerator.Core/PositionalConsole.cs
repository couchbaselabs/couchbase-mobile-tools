// 
// PositionalConsole.cs
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
using System.Text;

namespace LargeDatasetGenerator
{
    internal static class PositionalConsole
    {
        #region Constants

        private static readonly object Locker = new object();

        #endregion

        #region Public Methods

        public static void SetReserved(int lineCount)
        {
            Console.SetCursorPosition(0, Math.Max(Console.CursorTop, lineCount));
        }

        public static void WriteLine(string line, ConsoleColor? color = null)
        {
            lock (Locker) {
                if (color != null) {
                    Console.ForegroundColor = color.Value;
                }

                Console.WriteLine(line);

                if (color != null) {
                    Console.ResetColor();
                }
            }
        }

        public static void WriteLineAt(int left, int top, string line)
        {
            lock (Locker) {
                var oldLeft = Console.CursorLeft;
                var oldTop = Console.CursorTop;
                Console.SetCursorPosition(left, top);
                Console.WriteLine(line);
                Console.SetCursorPosition(oldLeft, oldTop);
            }
        }

        #endregion
    }

    internal sealed class ProgressOutput
    {
        #region Constants

        private static readonly char[] Indicators = new[] {'|', '/', '-', '\\'};

        #endregion

        #region Variables

        private readonly int _step;
        private int _callCount;

        #endregion

        #region Constructors

        public ProgressOutput(int step)
        {
            _step = step;
        }

        #endregion

        #region Public Methods

        public string Render(int completed, int total)
        {
            if (completed == total) {
                return " 　[==========] 100%";
            }

            _callCount++;
            char indicator = Indicators[(_callCount / _step) % Indicators.Length];
            int blockCount = (int) Math.Floor((double) completed / total * 10.0);
            var sb = new StringBuilder("[");
            for (int i = 0; i < 10; i++) {
                sb.Append(i < blockCount ? "=" : " ");
            }

            sb.AppendFormat("] {0}%", (int)((double)completed / total * 100.0));
            return $"{indicator} {sb}";
        }

        #endregion
    }
}