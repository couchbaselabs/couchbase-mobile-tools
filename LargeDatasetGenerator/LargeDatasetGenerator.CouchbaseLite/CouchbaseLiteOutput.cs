// 
// CouchbaseLiteOutput.cs
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
using System.Collections.Generic;
using System.Threading.Tasks;

using Couchbase.Lite;

using LargeDatasetGenerator.Output;

using McMaster.Extensions.CommandLineUtils;

namespace LargeDatasetGenerator.CouchbaseLite
{
    /// <summary>
    /// An <see cref="IJsonOutput" /> implementation that writes to a Couchbase
    /// Lite database
    /// </summary>
    public class CouchbaseLiteOutput : IJsonOutput, IDisposable
    {
        #region Variables

        private Database _database;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string ExpectedArgs { get; } = "--name <NAME> [--path <PATH>]";
        
        /// <inheritdoc />
        public string Name { get; } = "couchbase_lite";

        #endregion

        #region Constructors

        static CouchbaseLiteOutput()
        {
            Couchbase.Lite.Support.NetDesktop.Activate();
        }

        #endregion

        #region IDisposable
        
        /// <inheritdoc />
        public void Dispose()
        {
            _database.Dispose();
            _database = null;
        }

        #endregion

        #region IJsonOutput
        
        /// <inheritdoc />
        public bool PreloadArgs(string[] args)
        {
            var subApp = new CommandLineApplication();

            var name = subApp.Option<string>("--name <NAME>", "The database name to use for saving",
                CommandOptionType.SingleValue);
            name.IsRequired();

            var path = subApp.Option<string>("--path <PATH>", "The path to save the database in (optional)",
                CommandOptionType.SingleValue);

            subApp.OnExecute(() =>
            {
                var dbName = name.ParsedValue;
                DatabaseConfiguration config = null;
                if (path.HasValue()) {
                    config = new DatabaseConfiguration {Directory = path.ParsedValue};
                }

                _database = new Database(dbName, config);
            });

            return subApp.Execute(args) == 0;
        }
        
        /// <inheritdoc />
        public Task WriteBatchAsync(IEnumerable<IDictionary<string, object>> json)
        {
            return Task.Factory.StartNew(() =>
            {
                _database.InBatch(() =>
                {
                    foreach (var content in json) {
                        var contentCopy = new Dictionary<string, object>(content);
                        var id = contentCopy["_id"] as string;
                        contentCopy.Remove("_id");

                        using (var doc = new MutableDocument(id, contentCopy)) {
                            _database.Save(doc);
                        }
                    }
                });
            });
        }

        #endregion
    }
}