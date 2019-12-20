// 
// LogLevel.cs
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

namespace Couchbase.Lite.Binary.Log
{
    /// <summary>
    /// An enumerator of valid Couchbase Lite log levels
    /// </summary>
    public enum LogLevel
    {
        /// <summary>
        /// Only present in debug builds, noisy low level stuff
        /// </summary>
        Debug,

        /// <summary>
        /// The maximum verbosity for release builds
        /// </summary>
        Verbose,

        /// <summary>
        /// Informational lines about what is happening during
        /// the program run
        /// </summary>
        Info,

        /// <summary>
        /// Things that should be examined, but will not necessarily cause
        /// harm on their own
        /// </summary>
        Warn,

        /// <summary>
        /// Most likely something bad has happened
        /// </summary>
        Error
    }
}