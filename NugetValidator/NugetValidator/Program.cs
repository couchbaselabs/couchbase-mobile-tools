// See https://aka.ms/new-console-template for more information
using Couchbase.Lite;
using Spectre.Console;
using System.Reflection;

var version = typeof(Database).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()!.InformationalVersion;

AnsiConsole.MarkupLine("[bold]Found version {0}[/]", version);
if (args.Length > 0) {
    AnsiConsole.MarkupLine("[bold]Expecting version {0}[/]", args[0]);
    var equal = args[0] == version;
    if(equal) {
        AnsiConsole.MarkupLine(":check_mark: [green]All good![/]");
    } else {
        AnsiConsole.MarkupLine(":no_entry: [red]Versions don't match[/]");
    }
} else {
    AnsiConsole.MarkupLine(":red_question_mark: [yellow]No input version passed[/]");
}
