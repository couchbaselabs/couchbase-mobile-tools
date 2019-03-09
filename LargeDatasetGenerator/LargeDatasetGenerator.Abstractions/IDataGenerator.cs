// 
// IDataGenerator.cs
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

using System.Threading.Tasks;

namespace LargeDatasetGenerator.Generator
{
    /// <summary>
    /// An interface for generating values to insert into a set of output data.
    /// </summary>
    public interface IDataGenerator
    {
        #region Properties

        /// <summary>
        /// Gets a description of what this generator does for informational purposes
        /// </summary>
        string Description { get; }

        /// <summary>
        /// Gets the signature expected for this generator for informational purposes
        /// </summary>
        string Signature { get; }

        #endregion

        #region Public Methods

        /// <summary>
        /// Generates a new arbitrary value depending on the object's functionality
        /// </summary>
        /// <returns>An awaitable task holding the result</returns>
        Task<object> GenerateValueAsync();

        #endregion
    }
}