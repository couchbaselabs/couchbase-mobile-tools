using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading.Tasks;

namespace LargeDatasetGenerator.Extensions
{
    internal static class Extensions
    {
        #region Public Methods

        public static long NextInt64(this Random r, long min, long max)
        {
            if (max <= Int32.MaxValue && min >= Int32.MinValue) {
                return r.Next((int) min, (int) max);
            }

            var ret = (decimal) r.NextDouble();
            ret *= (max - min);
            ret += min;
            return (long) ret;
        }

        public static void AddRange<T>(this IList<T> collection, IEnumerable<T> range)
        {
            foreach (var item in range) {
                collection.Add(item);
            }
        }

        #endregion
    }
}