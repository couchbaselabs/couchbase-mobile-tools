// 
// LargeDatasetGenerator.cs
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
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

using LargeDatasetGenerator.Extensions;
using LargeDatasetGenerator.Generator;
using LargeDatasetGenerator.Output;

using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace LargeDatasetGenerator
{
    /// <summary>
    /// An interface that adds advanced behavioral options into <see cref="LargeDatasetGenerator"/>
    /// </summary>
    public interface ILargeDatasetConfiguration
    {
        #region Properties

        /// <summary>
        /// Gets or sets how many parallel jobs to run when creating JSON output
        /// </summary>
        int CreateJobCount { get; set; }

        /// <summary>
        /// Gets or sets how much to adjust the size of the internal queue between creation
        /// and writing.  As this value increases more memory will be used, but
        /// creation will be allowed to continue for longer before being throttled
        /// by write jobs
        /// </summary>
        double QueueSizeMultiplier { get; set; }

        /// <summary>
        /// Gets or sets how many parallel jobs to run when writing JSON output
        /// </summary>
        int WriteJobCount { get; set; }

        #endregion
    }

    /// <summary>
    /// A factory for creating the default <see cref="ILargeDatasetConfiguration" />
    /// implementation
    /// </summary>
    public static class LargeDatasetConfiguration
    {
        #region Public Methods

        /// <summary>
        /// Gets
        /// </summary>
        /// <returns></returns>
        public static ILargeDatasetConfiguration CreateDefault() => new Impl();

        #endregion

        private sealed class Impl : ILargeDatasetConfiguration
        {
            #region Properties
            
            public int CreateJobCount { get; set; } = Environment.ProcessorCount / 2;
            
            public double QueueSizeMultiplier { get; set; } = 1.0;
            
            public int WriteJobCount { get; set; } = Environment.ProcessorCount;

            
            #endregion
        }
    }

    /// <summary>
    /// A class which will generate a large set of random data based on an input from a template.
    /// </summary>
    public sealed class LargeDatasetGenerator
    {
        #region Constants

        private static readonly IList<IJsonOutput> AvailableOutputs = new List<IJsonOutput>();

        private static readonly ConcurrentDictionary<string, IDataGenerator> Generators
            = new ConcurrentDictionary<string, IDataGenerator>();

        #endregion

        #region Variables

        private readonly ILargeDatasetConfiguration _configuration;

        private int _countDigits;

        private BlockingCollection<IDictionary<string, object>> _jsonQueue;
        private int _totalCreated;
        private int _totalWritten;

        #endregion

        #region Properties

        /// <summary>
        /// Gets or sets the number of documents to write in one batch
        /// </summary>
        public int BatchSize { get; set; } = 100;

        /// <summary>
        /// Gets or sets the number of documents to create in total
        /// </summary>
        public int Count { get; set; } = 1000;

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="configuration">Optional configuration containing advanced parameters</param>
        public LargeDatasetGenerator(ILargeDatasetConfiguration configuration = null)
        {
            _configuration = configuration ?? LargeDatasetConfiguration.CreateDefault();
        }

        #endregion

        #region Public Methods

        /// <summary>
        /// Generates the data and writes it to the specified output.  This method is
        /// synchronous but the actual work is asynchronous.  This method will return when
        /// all work is done.
        /// </summary>
        /// <param name="input">The filename of the file containing the template input</param>
        /// <param name="outputType">The type of output to create (For a list refer to <seealso cref="ListOutput"/>)</param>
        /// <param name="extraArgs">The arguments needed for the specified output type (For a list refer to <seealso cref="ListOutput"/>)(</param>
        public void Generate(string input, string outputType, string[] extraArgs = null)
        {
            var progress = new ProgressOutput(500);
            Setup();

            var outputObject = AvailableOutputs.FirstOrDefault(x => x.Name == outputType);
            if (outputObject == null) {
                throw new InvalidOperationException(
                    $"Output type '{outputType}' not found in program or loaded plugins");
            }

            if (!outputObject.PreloadArgs(extraArgs ?? new string[0])) {
                Console.ForegroundColor = ConsoleColor.DarkRed;
                PositionalConsole.WriteLine($"Failed to load arguments into '{outputType}' output");
                Console.ResetColor();
                throw new ApplicationException($"Failed to load arguments into '{outputType}' output");
            }

            var tasks = StartConsumers(outputObject);
            var template = File.ReadAllText(input).Replace("\r\n", "\n");
            var templateObject = JsonConvert.DeserializeObject<IReadOnlyDictionary<string, object>>(template);
            var parallelOpts = new ParallelOptions
            {
                MaxDegreeOfParallelism = _configuration.CreateJobCount
            };

            var exceptions = new List<Exception>();
            Parallel.For(0, Count, parallelOpts, async (i, state) =>
            {
                if (state.IsExceptional) {
                    return;
                }

                var result = new Dictionary<string, object>();
                try {
                    await ApplyTemplate(templateObject, result).ConfigureAwait(false);
                } catch (Exception e) {
                    state.Stop();
                    exceptions.Add(e);
                }

                var newVal = Interlocked.Increment(ref _totalCreated);
                var progressOutput = progress.Render(newVal, Count);

                PositionalConsole.WriteLineAt(0, 0,
                    $"Created {newVal.ToString($"D{_countDigits}")} of {Count}...{progressOutput}");
                _jsonQueue.Add(result);
            });

            if (exceptions.Any()) {
                var agg = new AggregateException("Error during JSON generation", exceptions);
                PositionalConsole.WriteLine($"Error during processing: {agg}", ConsoleColor.DarkRed);
            } else {
                PositionalConsole.WriteLine("Finished adding, waiting for write jobs...");
            }

            _jsonQueue.CompleteAdding();
            Task.WaitAll(tasks);

            PositionalConsole.WriteLine("Write jobs finished!");
            (outputObject as IDisposable)?.Dispose();
        }

        public void ListGenerators()
        {
            Console.WriteLine("The following generators are available for processing");
            Console.WriteLine("Arguments followed by equal signs have a default value and are optional");
            Console.WriteLine();
            foreach (var x in DataGenerator.TypeMap) {
                try {
                    var generator = Activator.CreateInstance(x.Value) as IDataGenerator;
                    Console.WriteLine($"\t{x.Key}: {generator.Description}");
                    Console.WriteLine($"\t{generator.Signature}");
                    Console.WriteLine();
                } catch (MissingMethodException) {
                    Console.WriteLine(
                        $"\t{x.Key}: information not available because it has no parameterless constructor");
                    Console.WriteLine();
                }
            }
        }

        /// <summary>
        /// Lists the outputs that were found in the loaded assemblies and
        /// plugins that the executing application was compiled against.
        /// </summary>
        public void ListOutput()
        {
            foreach (var output in PluginLoader.OutputTypes.Select(Activator.CreateInstance).Cast<IJsonOutput>()) {
                Console.WriteLine(output.Name);
                PositionalConsole.WriteLine($"\t{output.ExpectedArgs}");
                Console.WriteLine();
            }
        }

        #endregion

        #region Private Methods

        private static IDataGenerator GetOrCreate(string generatorText)
        {
            return Generators.GetOrAdd(generatorText, x =>
            {
                var argStart = generatorText.IndexOf('(');
                var functionName = generatorText.Substring(0, argStart);
                var argList = generatorText.Substring(argStart);
                var generator = DataGenerator.Generator(functionName, argList);
                Generators[generatorText] = generator;
                return generator;
            });
        }

        private async Task ApplyRepeatTemplate(IReadOnlyDictionary<string, object> template, IList<object> result)
        {
            var key = template.Keys.First(x => x.StartsWith("repeat"));
            var argStart = key.IndexOf('(');
            var parser = new RepeatGeneratorArgs(key.Substring(argStart));
            var (min, max) = parser.Parse();
            var repeatCount = ThreadSafeRandom.Next(min, max);
            var dummyTemplate = Enumerable.Repeat(template[key], repeatCount);
            await ApplyTemplate(dummyTemplate.ToList(), result);
        }

        private async Task ApplyTemplate(IReadOnlyDictionary<string, object> template,
            IDictionary<string, object> result)
        {
            foreach (var entry in template) {
                if (entry.Value is string s) {
                    var generated = await SearchAndReplace(s).ConfigureAwait(false);
                    result[entry.Key] = generated;
                } else if (entry.Value is JObject jObj) {
                    var subObject = new Dictionary<string, object>();
                    await ApplyTemplate(jObj.ToObject<IReadOnlyDictionary<string, object>>(), subObject)
                        .ConfigureAwait(false);
                    result[entry.Key] = subObject;
                } else if (entry.Value is JArray jArr) {
                    var subObject = new List<object>();
                    await ApplyTemplate(jArr.ToObject<IReadOnlyList<object>>(), subObject).ConfigureAwait(false);
                    result[entry.Key] = subObject;
                } else {
                    result[entry.Key] = entry.Value;
                }
            }
        }

        private async Task ApplyTemplate(IReadOnlyList<object> template, IList<object> result)
        {
            foreach (var entry in template) {
                if (entry is string s) {
                    var generated = await SearchAndReplace(s).ConfigureAwait(false);
                    result.Add(generated);
                } else if (entry is JObject jObj) {
                    await ApplyTemplateFunctions(jObj.ToObject<IReadOnlyDictionary<string, object>>(), result)
                        .ConfigureAwait(false);
                } else if (entry is JArray jArr) {
                    var subObject = new List<object>();
                    await ApplyTemplate(jArr.ToObject<IReadOnlyList<object>>(), subObject).ConfigureAwait(false);
                    result.Add(subObject);
                } else {
                    result.Add(entry);
                }
            }
        }

        private async Task ApplyTemplateFunctions(IReadOnlyDictionary<string, object> template, IList<object> result)
        {
            var special = false;
            foreach (var entry in template) {
                if (entry.Key.StartsWith("repeat")) {
                    special = true;
                    break;
                }
            }

            if (!special) {
                var subObject = new Dictionary<string, object>();
                await ApplyTemplate(template, subObject).ConfigureAwait(false);
                result.Add(subObject);
            } else {
                await ApplyRepeatTemplate(template, result).ConfigureAwait(false);
            }
        }

        private async Task ConsumeJson(object obj)
        {
            var output = (IJsonOutput) obj;
            var taken = new List<IDictionary<string, object>>();
            var progress = new ProgressOutput(1);
            while (!_jsonQueue.IsCompleted) {
                if (_jsonQueue.TryTake(out var next)) {
                    taken.Add(next);
                    if (taken.Count == BatchSize) {
                        try {
                            await WriteJson(output, progress, taken).ConfigureAwait(false);
                        } catch (Exception e) {
                            Console.ForegroundColor = ConsoleColor.DarkRed;
                            PositionalConsole.WriteLine($"Error during output write: {e.InnerException ?? e}");
                            Console.ResetColor();
                            return;
                        }
                    }
                }
            }

            if (taken.Any()) {
                try {
                    await WriteJson(output, progress, taken).ConfigureAwait(false);
                } catch (Exception e) {
                    Console.ForegroundColor = ConsoleColor.DarkRed;
                    PositionalConsole.WriteLine($"Error during output write: {e.InnerException ?? e}");
                    Console.ResetColor();
                }
            }
        }

        private async Task<object> SearchAndReplace(string source)
        {
            var finalString = new StringBuilder(source);
            var matches = Regex.Matches(source, "\\{\\{(.*?)\\}\\}");
            var standalone = source.StartsWith("{{") && source.EndsWith("}}");
            object generated = null;
            for (int i = matches.Count - 1; i >= 0; i--) {
                var generatorText = matches[i].Groups[1].Value;
                var generator = GetOrCreate(generatorText);
                generated = await generator.GenerateValueAsync().ConfigureAwait(false);

                if (!standalone) {
                    finalString = finalString.Remove(matches[i].Index, matches[i].Length)
                        .Insert(matches[i].Index, generated);
                }
            }

            return standalone ? generated : finalString.ToString();
        }

        private void Setup()
        {
            _jsonQueue?.Dispose();
            _jsonQueue =
                new BlockingCollection<IDictionary<string, object>>(
                    (int) (BatchSize * _configuration.WriteJobCount * _configuration.QueueSizeMultiplier));
            Console.CursorVisible = false;
            Console.Clear();
            PositionalConsole.SetReserved(3);
            PositionalConsole.WriteLine(
                $"Using values CreateJobCount/{_configuration.CreateJobCount}, WriteJobCount/{_configuration.WriteJobCount}, QueueSizeMultiplier/{_configuration.QueueSizeMultiplier}");
            var tmpCount = Count;
            while (tmpCount > 0) {
                _countDigits++;
                tmpCount /= 10;
            }

            AvailableOutputs.AddRange(PluginLoader.OutputTypes.Select(Activator.CreateInstance).Cast<IJsonOutput>());
        }

        private Task[] StartConsumers(IJsonOutput output)
        {
            var count = Environment.ProcessorCount * 2;
            var tasks = new Task[count];
            for (int i = 0; i < count; i++) {
                tasks[i] = Task.Factory.StartNew(ConsumeJson, output, TaskCreationOptions.LongRunning).Unwrap();
            }

            return tasks;
        }

        private async Task WriteJson(IJsonOutput output, ProgressOutput progress,
            IList<IDictionary<string, object>> taken)
        {
            await output.WriteBatchAsync(taken).ConfigureAwait(false);
            var newVal = Interlocked.Add(ref _totalWritten, taken.Count);
            var progressOutput = progress.Render(newVal, Count);
            PositionalConsole.WriteLineAt(0, 1,
                $"Written {newVal.ToString($"D{_countDigits}")} of {Count}...{progressOutput}");
            taken.Clear();
        }

        #endregion
    }
}