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

using System.ComponentModel.DataAnnotations;

using McMaster.Extensions.CommandLineUtils;

namespace LogMergeTool
{
    public enum OutputFormat
    {
        SingleFile,
        SyncGateway,
        LogLady
    }

    partial class Program
    {
        #region Properties

        [Option("--force", Description = "Automatically overwrite existing log files", ShortName = "f")]
        public bool Force { get; set; }

        [Required]
        [DirectoryExists]
        [Option("--log-path <PATH>", Description = "The path to the folder containing log files to merge",
            ShortName = "p")]
        public string LogPath { get; set; }

        [Required]
        [Option("--output <PATH>",
            Description = "The file to write the results to (or base file name for multi file results)",
            ShortName = "o")]
        public string Output { get; set; }

        [Option("--output-type <TYPE>", Description = "The format to write the output as (default SingleFile)",
            ShortName = "t")]
        public OutputFormat OutputType { get; set; } = OutputFormat.SingleFile;

        #endregion
    }
}