using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using McMaster.Extensions.CommandLineUtils;

using Newtonsoft.Json;

namespace LargeDatasetGenerator.Output
{
    /// <summary>
    /// An output object that writes to JSON files.  Each file will contain the batch size
    /// number of entries.
    /// </summary>
    public sealed class JsonFileOutput : IJsonOutput
    {
        #region Constants

        /// <summary>
        /// Used by the core library for a default output type
        /// </summary>
        public const string NameConst = "file";

        #endregion

        #region Variables

        private int _currentBatch = 1;
        private string _filename;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string ExpectedArgs { get; } = "--filename <OUTPUT>";

        /// <inheritdoc />
        public string Name { get; } = NameConst;

        #endregion

        #region IJsonOutput

        /// <inheritdoc />
        public bool PreloadArgs(string[] args)
        {
            var subApp = new CommandLineApplication();

            var filename = subApp.Option<string>("--filename|-f <file>", "The file template to write JSON data into",
                CommandOptionType.SingleValue);
            filename.IsRequired();

            subApp.OnExecute(() => { _filename = Path.GetFileNameWithoutExtension(filename.ParsedValue); });

            return subApp.Execute(args) == 0;
        }

        /// <inheritdoc />
        public Task WriteBatchAsync(IEnumerable<IDictionary<string, object>> json)
        {
            var nextBatch = Interlocked.Increment(ref _currentBatch);
            var filename = $"{_filename}_{nextBatch}.json";
            return Task.Factory.StartNew(() =>
            {
                var serialized = JsonConvert.SerializeObject(json, Formatting.Indented);
                File.WriteAllText(filename, serialized, Encoding.UTF8);
            });
        }

        #endregion
    }
}