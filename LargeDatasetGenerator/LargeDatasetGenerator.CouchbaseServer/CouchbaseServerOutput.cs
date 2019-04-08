// 
// CouchbaseServerOutput.cs
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
using System.Linq;
using System.Threading.Tasks;

using Couchbase;
using Couchbase.Authentication;
using Couchbase.Configuration.Client;
using Couchbase.Core;

using LargeDatasetGenerator.Output;

using McMaster.Extensions.CommandLineUtils;

namespace LargeDatasetGenerator.CouchbaseServer
{
    public sealed class CouchbaseServerOutput : IJsonOutput, IDisposable
    {
        #region Variables

        private IBucket _bucket;
        private string _bucketName;

        private string _password;
        private Uri _url;
        private string _username;

        #endregion

        #region Properties

        public string ExpectedArgs { get; } = "--url <URL> --username <USER> --password <PASS> --bucket <BUCKET_NAME>";

        public string Name { get; } = "couchbase_server";

        #endregion

        #region IDisposable

        public void Dispose()
        {
            _bucket.Dispose();
        }

        #endregion

        #region IJsonOutput

        public bool PreloadArgs(string[] args)
        {
            var subApp = new CommandLineApplication();

            var url = subApp.Option<string>("--url <url>", "The file template to write JSON data into",
                CommandOptionType.SingleValue);
            url.IsRequired();
            url.Validators.Add(new UrlValidation());

            var username = subApp.Option<string>("--username <user>", "The username to log in as",
                CommandOptionType.SingleValue);
            username.IsRequired();

            var password = subApp.Option<string>("--password <pass>", "The password to log in with",
                CommandOptionType.SingleValue);
            password.IsRequired();

            var bucket = subApp.Option<string>("--bucket <bucket>", "The bucket to use for writing data",
                CommandOptionType.SingleValue);
            bucket.IsRequired();

            subApp.OnExecute(() =>
            {
                _url = new Uri(url.ParsedValue, UriKind.Absolute);
                _username = username.ParsedValue;
                _password = password.ParsedValue;
                _bucketName = bucket.ParsedValue;
            });

            return subApp.Execute(args) == 0;
        }

        public Task WriteBatchAsync(IEnumerable<IDictionary<string, object>> json)
        {
            if (_bucket == null) {
                var cluster = new Cluster(new ClientConfiguration
                {
                    Servers = new List<Uri> {_url}
                });

                var authenticator = new PasswordAuthenticator(_username, _password);
                cluster.Authenticate(authenticator);
                _bucket = cluster.OpenBucket(_bucketName);
            }

            IEnumerable<IDocument<IDictionary<string, object>>> documents = json.Select(x =>
            {
                var retVal = new Document<IDictionary<string, object>>
                {
                    Id = x["_id"] as string,
                    Content = new Dictionary<string, object>(x)
                };

                retVal.Content.Remove("_id");

                return retVal;
            });


            return _bucket.InsertAsync(documents.ToList());
        }

        #endregion
    }
}