using System;

using LargeDatasetGenerator.Extensions;

namespace LargeDatasetGenerator.Generator
{
    internal static class ThreadSafeRandom
    {
        #region Constants

        private static readonly object Locker = new object();
        private static readonly Random Rng = new Random();

        #endregion

        #region Public Methods

        public static int Next(int min, int max)
        {
            lock (Locker) {
                return Rng.Next(min, max);
            }
        }

        public static double NextDouble()
        {
            lock (Locker) {
                return Rng.NextDouble();
            }
        }

        public static long NextInt64(long min, long max)
        {
            lock (Locker) {
                return Rng.NextInt64(min, max);
            }
        }

        #endregion
    }
}