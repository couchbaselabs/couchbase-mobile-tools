using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Text.Json;
using RetentionUpdater.JsonModel;
using Spectre.Console;
using Spectre.Console.Cli;
using Spectre.Console.Json;

namespace RetentionUpdater
{
    internal class NewVersionCommand : AsyncCommand<NewVersionCommand.Settings>
    {
        public override async Task<int> ExecuteAsync(CommandContext context, Settings settings)
        {
            if (Program.ProgetApiKey == string.Empty) {
                AnsiConsole.MarkupLine("[red]ProgetApiKey not specified, please run dotnet user-secrets set ProgetApiKey (key)[/]");
                return 1;
            }

            var feed = await Program.GetFeed(settings.FeedName);
            if(settings.Verbose) {
                AnsiConsole.MarkupLine($"[gray]Feed {settings.FeedName} is of type {feed.feedType}[/]");
                AnsiConsole.MarkupLine($"[gray]\t...{feed.retentionRules.Count} retention rules found[/]");
            }

            var versionSplit = settings.Build.Split('.');
            var versionToAdd = string.Join(".", versionSplit[0], versionSplit[1], "*");
            foreach(var rule in feed.retentionRules) {
                if(rule.deleteVersions?.Contains(versionToAdd) == true) {
                    AnsiConsole.MarkupLine($"[yellow]WARNING: Version {settings.Build} already has a retention rule, giving up![/]");
                    return 2;
                }
            }

            var ruleToAdd = new RetentionRule(
                keepVersionsCount: 10,
                deleteVersions: [versionToAdd]
            );

            var newRules = new List<RetentionRule>(feed.retentionRules)
            {
                ruleToAdd
            };

            if(settings.Verbose || settings.DryRun) {
                AnsiConsole.MarkupLine(settings.DryRun
                    ? "[yellow]Dry run enabled, no changes will actually be made![/]"
                    : "[gray]Performing the following changes...[/]");

                AnsiConsole.MarkupLine("[green]ADDING:[/]");
                AnsiConsole.Write(new JsonText(JsonSerializer.Serialize(ruleToAdd, RetentionRuleContext.Default.RetentionRule)));
            }

            if(settings.DryRun) {
                return 0;
            }

            var updatedFeed = new FeedEntry(feed.name, feed.feedType, newRules);
            await Program.UpdateFeed(settings.FeedName, updatedFeed);

            return 0;
        }

        [SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
        [SuppressMessage("ReSharper", "AutoPropertyCanBeMadeGetOnly.Global")]
        public sealed class Settings : CommandSettings
        {
            [CommandOption("-f|--feed-name")]
            [Description("The name of the ProGet feed to update")]
            public string FeedName { get; init; } = string.Empty;

            [CommandOption("-b|--build")]
            [Description("The new version to add")]
            public string Build { get; init; } = string.Empty;

            [CommandOption("--verbose")]
            [Description("Turn on more verbose output")]
            public bool Verbose { get; init; } = false;

            [CommandOption("-d|--dry-run")]
            [Description("Shows what the program will change without actually changing it")]
            public bool DryRun { get; init; } = false;

            public override ValidationResult Validate()
            {
                if (FeedName == string.Empty) {
                    return ValidationResult.Error("Missing required option --feed-name");
                }

                if (Build == string.Empty) {
                    return ValidationResult.Error("Missing required option --build");
                }

                var versionParts = Build.Split('.');
                if (versionParts.Length != 3) {
                    return ValidationResult.Error($"Invalid version {Build}, must be formatted as x.y.z");
                }

                if (versionParts[2] != "*") {
                    AnsiConsole.MarkupLine($"[yellow]WARNING: Last component of version {Build} will be replaced with *[/]");
                }

                return ValidationResult.Success();
            }
        }
    }
}
