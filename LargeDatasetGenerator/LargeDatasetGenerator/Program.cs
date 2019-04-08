// 
// Program.cs
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
using System.IO;

using McMaster.Extensions.CommandLineUtils;

using Microsoft.Extensions.Configuration;

namespace LargeDatasetGenerator
{
    [Command(Name = "LargeDatasetGenerator", Description = "Generates data into a JSON accepting endpoint",
        ThrowOnUnexpectedArgument = false)]
    [HelpOption("-?")]
    partial class Program
    {
        #region Private Methods

        static void Main(string[] args)
        {
            try {
                CommandLineApplication.Execute<Program>(args);
            } finally {
                Console.CursorVisible = true;
            }
        }

        private void ExtractMultiplierValue(IConfiguration config, string key, Action<double> processor)
        {
            var value = config[key];
            if (value == null) {
                return;
            }

            var numericPortion = Double.Parse(value.TrimEnd('*'));
            if (value.EndsWith('*')) {
                numericPortion *= Environment.ProcessorCount;
            }

            Console.WriteLine($"Using custom value {key}: {value}...");
            processor(numericPortion);
        }

        private void ExtractValue<T>(IConfiguration config, string key, Action<T> processor) where T : IConvertible
        {
            var value = config[key];
            if (value == null) {
                return;
            }

            Console.WriteLine($"Using custom value {key}: {value}...");
            processor((T) Convert.ChangeType(value, typeof(T)));
        }

        private void OnExecute()
        {
            var configBuilder = new ConfigurationBuilder().AddEnvironmentVariables("LDG_");
            if (AdvancedFile != null) {
                configBuilder.AddIniFile(Path.GetFullPath(AdvancedFile), false);
            }

            var passedConfig = configBuilder.Build();
            var generatorConfig = LargeDatasetConfiguration.CreateDefault();
            ExtractMultiplierValue(passedConfig, "create_job_count", x => generatorConfig.CreateJobCount = (int) x);
            ExtractMultiplierValue(passedConfig, "write_job_count", x => generatorConfig.WriteJobCount = (int) x);
            ExtractValue<double>(passedConfig, "queue_size_multiplier", x => generatorConfig.QueueSizeMultiplier = x);

            var generator = new LargeDatasetGenerator(generatorConfig)
            {
                BatchSize = BatchSize,
                Count = Count
            };

            if (ListOutputs) {
                generator.ListOutput();
                return;
            }

            if (ListGenerators) {
                generator.ListGenerators();
                return;
            }

            generator.Generate(Input, OutputType, RemainingArguments);
        }

        #endregion
    }
}