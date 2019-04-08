// 
// Program_args.cs
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
using System.ComponentModel.DataAnnotations;

using LargeDatasetGenerator.Output;

using McMaster.Extensions.CommandLineUtils;

namespace LargeDatasetGenerator
{
    [ProgramArgumentValidation]
    partial class Program
    {
        #region Properties

        [Option("--advanced <INI_FILENAME>", Description = "An ini file containing advanced configuration options")]
        public string AdvancedFile { get; set; }

        [Option("--batch-size <SIZE>", Description = "The number of entries to process as one unit (default 100)",
            ShortName = "b")]
        [Range(1, Int32.MaxValue)]
        public int BatchSize { get; set; } = 100;

        [Option("--count <COUNT>", Description = "The number of entries to generate from the template (default 1000)",
            ShortName = "c")]
        [Range(1, Int32.MaxValue)]
        public int Count { get; set; } = 1000;

        [FileExists]
        [Option("--input <FILE>", Description = "The file to read the document template from", ShortName = "i")]
        public string Input { get; set; }

        [Option("--list-generators", Description = "List the available generator functions for the template JSON")]
        public bool ListGenerators { get; set; }

        [Option("--list-output", Description = "List the available output methods for writing JSON")]
        public bool ListOutputs { get; set; }

        [Option("--output <TYPE>", Description = "The way to save the resulting JSON", ShortName = "o")]
        public string OutputType { get; set; } = JsonFileOutput.NameConst;

        public string[] RemainingArguments { get; }

        #endregion
    }

    internal sealed class ProgramArgumentValidationAttribute : ValidationAttribute
    {
        #region Overrides

        protected override ValidationResult IsValid(object value, ValidationContext validationContext)
        {
            if (value is Program p) {
                if (p.ListOutputs || p.ListGenerators) {
                    return ValidationResult.Success;
                }

                if (p.Input == null) {
                    return new ValidationResult("Required argument --input missing");
                }
            }

            return ValidationResult.Success;
        }

        #endregion
    }
}