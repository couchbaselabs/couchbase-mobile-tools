using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;

namespace LogMergeTool.Outputs
{
    internal sealed class SyncGatewayOutput : ILogOutput
    {
        #region Variables

        private readonly FileLogOutput[] _writers = new FileLogOutput[5];

        #endregion

        #region Constructors

        public SyncGatewayOutput(string fileLogPath, bool overwrite)
        {
            var directory = Path.GetDirectoryName(fileLogPath) + Path.DirectorySeparatorChar;
            var extension = Path.GetExtension(fileLogPath);
            var baseFileName = Path.GetFileNameWithoutExtension(fileLogPath);

            foreach (var level in new[]
                {LogLevel.Debug, LogLevel.Verbose, LogLevel.Info, LogLevel.Warn, LogLevel.Error}) {
                var outPath = $"{directory}{baseFileName}-{level.ToString().ToLowerInvariant()}{extension}";
                _writers[(int) level] = new FileLogOutput(outPath, overwrite);
            }
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            foreach (var writer in _writers) {
                writer.Dispose();
            }
        }

        #endregion

        #region ILogOutput

        public async ValueTask WriteAsync(LogLine line)
        {
            foreach (var writer in _writers.Take((int) line.Level + 1)) {
                await writer.WriteAsync(line).ConfigureAwait(false);
            }
        }

        public async ValueTask WriteAsync(string line)
        {
            foreach (var writer in _writers) {
                await writer.WriteAsync(line).ConfigureAwait(false);
            }
        }

        #endregion
    }
}