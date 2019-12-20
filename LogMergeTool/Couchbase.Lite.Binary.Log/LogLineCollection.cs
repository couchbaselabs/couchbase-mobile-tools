// 
// LogLineCollection.cs
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
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;

namespace Couchbase.Lite.Binary.Log
{
    /// <summary>
    /// Stores a collection of log lines sorted by time
    /// </summary>
    public sealed class LogLineCollection : ICollection<LogLine>, IDisposable
    {
        #region Variables

        private readonly Guid _group = Guid.NewGuid();

        [NotNull] private readonly SortedDictionary<DateTimeOffset, List<LogLine>> _items
            = new SortedDictionary<DateTimeOffset, List<LogLine>>();

        private bool _normalized;

        #endregion

        #region Properties

        public int Count => this.Count();

        public bool IsReadOnly => false;

        #endregion

        #region Constructors

        ~LogLineCollection()
        {
            Dispose(false);
        }

        #endregion

        #region Private Methods

        private static uint AddIfNecessary(Dictionary<string, uint> tokenMap, string value, ref uint nextToken)
        {
            if (tokenMap.TryGetValue(value, out var id)) {
                return id;
            }

            tokenMap[value] = nextToken;
            return nextToken++;
        }

        private void Dispose(bool disposing)
        {
            TokenStringLogArgument.ClearGroup(_group);
        }

        private void Normalize()
        {
            if (_normalized) {
                return;
            }

            _normalized = true;
            uint nextToken = 0;
            Dictionary<string, uint> tokenMap = new Dictionary<string, uint>();
            foreach (var line in this) {
                line.Domain.Id = AddIfNecessary(tokenMap, line.Domain.Value, ref nextToken);
                line.MessageFormat.Id = AddIfNecessary(tokenMap, line.MessageFormat.Value, ref nextToken);
                for (int i = 0; i < (line.Arguments?.Count ?? 0); i++) {
                    var nextArg = line.Arguments[i];
                    if (nextArg is TokenStringLogArgument s) {
                        s.Value.Id = AddIfNecessary(tokenMap, s.Value.Value, ref nextToken);
                        s.AddToGroup(_group);
                    }
                }
            }
        }

        #endregion

        #region ICollection<LogLine>

        public void Add(LogLine item)
        {
            if (item == null) {
                throw new ArgumentNullException(nameof(item));
            }

            _normalized = false;
            if (!_items.TryGetValue(item.Time, out var lines)) {
                lines = new List<LogLine> {item};
                _items[item.Time] = lines;
            } else {
                lines.Add(item);
            }
        }

        public void Clear() => _items.Clear();

        public bool Contains(LogLine item)
        {
            if (item == null) {
                return false;
            }

            return _items.TryGetValue(item.Time, out var existing)
                   && existing.Contains(item);
        }


        public void CopyTo(LogLine[] array, int arrayIndex)
        {
            var i = arrayIndex;
            foreach (var line in this) {
                if (i < array.Length) {
                    array[i++] = line;
                }
            }
        }

        public bool Remove(LogLine item)
        {
            if (item == null) {
                return false;
            }

            var removed = _items.ContainsKey(item.Time) && _items[item.Time].Remove(item);
            if (removed) {
                _normalized = false;
            }

            return removed;
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        #endregion

        #region IEnumerable

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

        #endregion

        #region IEnumerable<LogLine>

        public IEnumerator<LogLine> GetEnumerator()
        {
            Normalize();
            foreach (var pair in _items) {
                foreach (var line in pair.Value) {
                    yield return line;
                }
            }
        }

        #endregion
    }
}