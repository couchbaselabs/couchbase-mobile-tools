using System;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

using Couchbase.Lite.Binary.Log;

namespace LogMergeTool.Outputs
{
    internal sealed class FileLogOutput : ILogOutput
    {
        #region Variables

        private readonly string _basePath;
        private readonly uint _maxSize;
        private readonly bool _overwrite;

        [NotNull] private StreamWriter _fileOut;
        private LogLine _lastLine;
        private uint _lastObjWritten;

        private int _nextFileOffset;

        #endregion

        #region Constructors

        public FileLogOutput(string logFilePath, bool overwrite)
        {
            var mode = overwrite ? FileMode.Create : FileMode.CreateNew;
            var stream = File.Open(logFilePath, mode, FileAccess.Write, FileShare.Read);
            _fileOut = new StreamWriter(stream, Encoding.UTF8, leaveOpen: false);
            _maxSize = GetMaxSize();
            _basePath = logFilePath;
            _overwrite = overwrite;
        }

        #endregion

        #region Private Methods

        private string Decorate(LogLine line)
        {
            if (line.Object?.Id > _lastObjWritten) {
                _lastObjWritten = line.Object.Id;
                return $"[{line.Domain.Value}]: {{{line.Object.Id}|{line.Object.Value}}} {line.FormattedMessage}";
            }

            if (line.Object?.Id > 0) {
                return $"[{line.Domain.Value}]: {{{line.Object.Id}}} {line.FormattedMessage}";
            }

            if (!String.IsNullOrEmpty(line.Domain?.Value)) {
                return $"[{line.Domain.Value}]: {line.FormattedMessage}";
            }

            return line.FormattedMessage;
        }

        private uint GetMaxSize()
        {
            var maxSizeString = Advanced.Configuration[ConfigurationKeys.SingleFile.MaxSize];
            if (maxSizeString == null) {
                return UInt32.MaxValue;
            }

            var regex = new Regex("(\\d+)([GgMmKk]?)");
            var match = regex.Match(maxSizeString);
            if (!match.Success) {
                Program.Warn($"Invalid advanced configuration {ConfigurationKeys.SingleFile.MaxSize} ({maxSizeString})");
                return UInt32.MaxValue;
            }

            if (!UInt32.TryParse(match.Groups[1].Value, out var numericSize)) {
                Program.Warn($"Invalid advanced configuration {ConfigurationKeys.SingleFile.MaxSize} ({maxSizeString})");
                return UInt32.MaxValue;
            }

            var multiplier = 1u;
            switch (match.Groups[2].Value.ToLowerInvariant()[0]) {
                case 'k':
                    multiplier = 1024;
                    break;
                case 'm':
                    multiplier = 1024 * 1024;
                    break;
                case 'g':
                    multiplier = 1024 * 1024 * 1024;
                    break;
            }

            return numericSize * multiplier;
        }

        private void RotateIfNecessary(int nextLen)
        {
            if (_fileOut.BaseStream.Position + nextLen > _maxSize) {
                var oldFileOut = Interlocked.Exchange(ref _fileOut, RotateWriter());
                var ignore = oldFileOut.DisposeAsync();
            }
        }

        private StreamWriter RotateWriter()
        {
            var mode = _overwrite ? FileMode.Create : FileMode.CreateNew;
            var nextOffset = Interlocked.Increment(ref _nextFileOffset);
            var newExtension = $".{nextOffset}{Path.GetExtension(_basePath)}";
            var stream = File.Open(Path.ChangeExtension(_basePath, newExtension), mode, FileAccess.Write,
                FileShare.Read);
            return new StreamWriter(stream, Encoding.UTF8);
        }

        #endregion

        #region IDisposable

        public void Dispose() => _fileOut.Dispose();

        #endregion

        #region ILogOutput

        public async ValueTask WriteAsync(LogLine line)
        {
            if (_lastLine != null && line.Time.Day != _lastLine.Time.Day) {
                await WriteAsync($"---- Date change to {line.Time.Date:dddd, MM/dd/yy} ----")
                    .ConfigureAwait(false);
            }

            RotateIfNecessary(line.FormattedMessage.Length);
            _lastLine = line;
            await _fileOut
                .WriteLineAsync($"{line.Level.ToString()[0]} {line.Time:HH:mm:ss.ffffff}| {Decorate(line)}")
                .ConfigureAwait(false);
        }

        public async ValueTask WriteAsync(string line)
        {
            RotateIfNecessary(line.Length);
            await _fileOut.WriteLineAsync(line).ConfigureAwait(false);
        }

        #endregion
    }
}