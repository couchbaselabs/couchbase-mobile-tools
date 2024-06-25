using System.Diagnostics.CodeAnalysis;
using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using Microsoft.Extensions.Configuration;
using RetentionUpdater.JsonModel;
using Spectre.Console.Cli;

namespace RetentionUpdater;

internal class Program
{
    private static readonly HttpClient HttpClient = new HttpClient();
    private const string ProgetUrl = "https://proget.sc.couchbase.com";

    public static string ProgetApiKey { get; private set; } = string.Empty;

    [DynamicDependency(DynamicallyAccessedMemberTypes.All, typeof(AddRCCommand))]
    [DynamicDependency(DynamicallyAccessedMemberTypes.All, typeof(NewVersionCommand))]
    [DynamicDependency(DynamicallyAccessedMemberTypes.All, "Spectre.Console.Cli.ExplainCommand", "Spectre.Console.Cli")]
    [DynamicDependency(DynamicallyAccessedMemberTypes.All, "Spectre.Console.Cli.XmlDocCommand", "Spectre.Console.Cli")]
    [DynamicDependency(DynamicallyAccessedMemberTypes.All, "Spectre.Console.Cli.VersionCommand", "Spectre.Console.Cli")]
    private static async Task<int> Main(string[] args)
    {
        var externalConfig = new ConfigurationBuilder()
            .AddUserSecrets<Program>()
            .Build();


        ProgetApiKey = externalConfig["ProgetApiKey"] ?? string.Empty;
        if(ProgetApiKey != string.Empty) {
            HttpClient.DefaultRequestHeaders.Add("X-ApiKey", ProgetApiKey);
        }

        var app = new CommandApp();
        app.Configure(config =>
        {
            config.AddCommand<AddRCCommand>("add-rc")
                .WithDescription("Record an RC version in ProGet on a specified feed, so that it is not deleted by retention rules")
                .WithExample("add-rc", "--feed-name", "CI", "--build", "3.2.0-b0063")
                .WithExample("add-rc", "--feed-name", "cimaven", "--build", "3.2.0-63");

            config.AddCommand<NewVersionCommand>("new-version")
                .WithDescription("Add a new version line to be tracked by ProGet retention rules (e.g. 3.2.*)")
                .WithExample("new-version", "--feed-name", "CI", "--build", "3.2.*");
        });

        return await app.RunAsync(args);
    }

    public static async Task<FeedEntry> GetFeed(string name)
    {
        var response = await HttpClient.GetAsync($"{ProgetUrl}/api/management/feeds/get/{name}");
        if (response.StatusCode == HttpStatusCode.NotFound) {
            throw new InvalidOperationException($"Feed {name} does not exist!");
        } else if(!response.IsSuccessStatusCode) {
            throw new ApplicationException($"HTTP error {response.StatusCode} getting feed {name}!");
        }

        var content = await response.Content.ReadAsStreamAsync();
        return await JsonSerializer.DeserializeAsync(content, FeedEntryContext.Default.FeedEntry);
    }

    public static async Task UpdateFeed(string name, FeedEntry updatedEntry)
    {
        var response = await HttpClient.PostAsJsonAsync($"{ProgetUrl}/api/management/feeds/update/{name}", updatedEntry,
            FeedEntryContext.Default.FeedEntry);
        if (response.StatusCode == HttpStatusCode.NotFound) {
            throw new InvalidOperationException($"Feed {name} does not exist!");
        } else if (!response.IsSuccessStatusCode) {
            throw new ApplicationException($"HTTP error {response.StatusCode} updating feed {name}!");
        }
    }
}