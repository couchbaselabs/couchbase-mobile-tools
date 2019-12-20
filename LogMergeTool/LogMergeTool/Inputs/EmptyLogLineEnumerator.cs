using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;

namespace LogMergeTool.Inputs
{
    sealed partial class LogLineEnumerator
    {
        // If the log cannot be enumerated, this dummy class will be used
        private sealed class EmptyLogLineEnumerator : IAsyncEnumerator<LogLine>
        {
            public ValueTask DisposeAsync()
            {
                return new ValueTask(Task.CompletedTask);
            }

            public ValueTask<bool> MoveNextAsync()
            {
                return new ValueTask<bool>(false);
            }

            public LogLine Current => null;
        }
    }
}
