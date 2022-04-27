using System;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;

namespace LogMergeTool
{
    interface ILogOutput : IDisposable
    {
        #region Public Methods

        ValueTask WriteAsync(LogLine line);

        ValueTask WriteAsync(string line);

        #endregion
    }
}