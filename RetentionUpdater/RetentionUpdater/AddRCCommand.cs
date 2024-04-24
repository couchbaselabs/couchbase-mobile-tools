using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Text.Json;
using RetentionUpdater.JsonModel;
using Spectre.Console;
using Spectre.Console.Cli;
using Spectre.Console.Json;

namespace RetentionUpdater
{
    internal class AddRCCommand : AsyncCommand<AddRCCommand.Settings>
    {
        public override async Task<int> ExecuteAsync(CommandContext context, Settings settings)
        {
            if (Program.ProgetApiKey == string.Empty) {
                AnsiConsole.MarkupLine("[red]ProgetApiKey not specified, please run dotnet user-secrets set ProgetApiKey (key)[/]");
                return 1;
            }

            var feed = await Program.GetFeed(settings.FeedName);
            if (settings.Verbose) {
                AnsiConsole.MarkupLine($"[gray]Feed {settings.FeedName} is of type {feed.feedType}[/]");
                AnsiConsole.MarkupLine($"[gray]\t...{feed.retentionRules.Count} retention rules found[/]");
            }

            var versionSplit = settings.Build.Split('.');
            var versionToModify = string.Join(".", versionSplit[0], versionSplit[1], "*");

            var newRules = new List<RetentionRule>();
            RetentionRule? foundRule = null;
            RetentionRule? newRule = null;
            foreach(var rule in feed.retentionRules) {
                if(rule.deleteVersions?.Contains(versionToModify) == true) {
                    foundRule = rule;
                    var versionsToKeep = rule.keepVersions == null ? [] : new List<string>(rule.keepVersions);
                    versionsToKeep.Add(settings.Build);
                    newRule = rule with { keepVersions = versionsToKeep };
                    newRules.Add(newRule.Value);
                } else {
                    newRules.Add(rule);
                }
            }

            if(!foundRule.HasValue) {
                AnsiConsole.MarkupLine($"[yellow]WARNING: No suitable retention rule found to add RC {settings.Build} (looking for {versionToModify}), giving up![/]");
                return 2;
            }

            if(settings.Verbose || settings.DryRun) {
                AnsiConsole.MarkupLine(settings.DryRun
                    ? "[yellow]Dry run enabled, no changes will actually be made![/]"
                    : "[gray]Performing the following changes...[/]");

                AnsiConsole.MarkupLine("[red]BEFORE:[/]");
                AnsiConsole.Write(new JsonText(JsonSerializer.Serialize(foundRule, RetentionRuleContext.Default.RetentionRule)));
                AnsiConsole.MarkupLine("\n[green]AFTER:[/]");
                AnsiConsole.Write(new JsonText(JsonSerializer.Serialize(newRule, RetentionRuleContext.Default.RetentionRule)));
            }

            if(settings.DryRun) {
                return 0;
            }

            var updatedEntry = new FeedEntry(feed.name, feed.feedType, newRules);
            await Program.UpdateFeed(settings.FeedName, updatedEntry);

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
            [Description("The build to record as the RC")]
            public string Build { get; init; } = string.Empty;

            [CommandOption("-v|--verbose")]
            [Description("Turn on more verbose output")]
            public bool Verbose { get; init; } = false;

            [CommandOption("-d|--dry-run")]
            [Description("Shows what the program will change without actually changing it")]
            public bool DryRun { get; init; } = false;

            public override ValidationResult Validate()
            {
                if(FeedName == string.Empty) {
                    return ValidationResult.Error("Missing required option --feed-name");
                }

                if (Build == string.Empty) {
                    return ValidationResult.Error("Missing required option --build");
                }

                var versionAndBuild = Build.Split('-');
                if(versionAndBuild.Length != 2) {
                    return ValidationResult.Error($"Invalid build {Build}, must be formatted as x.y.z-[build]");
                }

                var versionParts = versionAndBuild[0].Split('.');
                if (versionParts.Length != 3) {
                    return ValidationResult.Error($"Invalid build {Build}, must be formatted as x.y.z-[build]");
                }

                return ValidationResult.Success();
            }
        }
    }
}
