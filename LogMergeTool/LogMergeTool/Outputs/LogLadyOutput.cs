using System;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Text;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;
using Couchbase.Lite.Binary.Log.Encoders;

namespace LogMergeTool.Outputs
{
    internal sealed class LogLadyOutput : ILogOutput
    {
        #region Variables

        [NotNull] private readonly ILogEncoder _logEncoder;
        [NotNull] private readonly StreamWriter _metaOut;

        #endregion

        #region Constructors

        public LogLadyOutput(string path, bool overwrite, DateTimeOffset start)
        {
            var mode = overwrite ? FileMode.Create : FileMode.CreateNew;
            var stream = File.Open(path, mode, FileAccess.Write, FileShare.Read);
            _logEncoder = LogEncoder.CreateEncoder(1, stream, start);

            var metaPath = Path.Combine(Path.GetDirectoryName(path), 
                Path.GetFileNameWithoutExtension(path) + "-meta.txt");
            stream = File.Open(metaPath, mode, FileAccess.Write, FileShare.Read);
            _metaOut = new StreamWriter(stream, Encoding.UTF8, leaveOpen: false);
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            _logEncoder.Dispose();
            _metaOut.Dispose();
        }

        #endregion

        #region ILogOutput

        public ValueTask WriteAsync(LogLine line) => _logEncoder.WriteAsync(line);

        public async ValueTask WriteAsync(string line) => await _metaOut.WriteLineAsync(line).ConfigureAwait(false);

        #endregion
    }
}