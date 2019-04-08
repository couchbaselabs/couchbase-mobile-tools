// 
// IJsonOutput.cs
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

using System.Collections.Generic;
using System.Threading.Tasks;

namespace LargeDatasetGenerator.Output
{
    /// <summary>
    /// An interface representing an object that can accept JSON to write
    /// to an arbitrary output
    /// </summary>
    public interface IJsonOutput
    {
        #region Properties

        /// <summary>
        /// Gets the arguments that this object expects when it is invoked
        /// </summary>
        string ExpectedArgs { get; }

        /// <summary>
        /// Gets the name of this object so that it can be identified via
        /// passed argument
        /// </summary>
        string Name { get; }

        #endregion

        #region Public Methods

        /// <summary>
        /// Preload the arguments from the command line to verify that they are
        /// all present and correct
        /// </summary>
        /// <param name="args">The arguments to process (in the form --[argname] [value])</param>
        /// <returns><c>true</c> if the arguments are all present and valid, otherwise <c>false</c></returns>
        bool PreloadArgs(string[] args);

        /// <summary>
        /// Writes a batch of JSON objects to the output
        /// </summary>
        /// <param name="json">The set of JSON objects to write</param>
        /// <returns>An awaitable task</returns>
        Task WriteBatchAsync(IEnumerable<IDictionary<string, object>> json);

        #endregion
    }
}