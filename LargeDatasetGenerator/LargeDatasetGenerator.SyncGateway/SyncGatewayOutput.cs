using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;
using LargeDatasetGenerator.Output;
using McMaster.Extensions.CommandLineUtils;
using Newtonsoft.Json;

namespace LargeDatasetGenerator.SyncGateway
{
    public sealed class SyncGatewayOutput : IJsonOutput, IDisposable
    {
        #region Variables

        private readonly HttpClient _httpClient = new HttpClient();

        private Uri _url;

        #endregion

        #region Properties

        public string ExpectedArgs { get; } = "--url <URL>";
        public string Name { get; } = "sync_gateway";

        #endregion

        #region IDisposable

        public void Dispose()
        {
            _httpClient.Dispose();
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

            subApp.OnExecute(() =>
            {
                var urlString = url.ParsedValue;
                if (!urlString.EndsWith("/")) {
                    urlString += "/";
                }

                _url = new Uri(urlString, UriKind.Absolute);
            });

            return subApp.Execute(args) == 0;
        }

        public async Task WriteBatchAsync(IEnumerable<IDictionary<string, object>> json)
        {
            var postBody = new Dictionary<string, object>
            {
                ["docs"] = json.ToList()
            };

            var content = new StringContent(JsonConvert.SerializeObject(postBody));
            content.Headers.ContentType = new MediaTypeHeaderValue("application/json");
            await _httpClient.PostAsync(new Uri(_url, "_bulk_docs"),
                content).ConfigureAwait(false);
        }

        #endregion
    }
}