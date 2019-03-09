# Adding Output Types

The interface for writing JSON to output is [IJsonOutput](LargeDatasetGenerator.Abstractions/IJsonOutput.cs) in the LargetDatasetGenerator.Abstractions assembly.  Your plugin will reference that assembly, compile, and then the executing program will reference your plugin.  Here is a walkthrough of how the default one works:

```c#
public sealed class JsonFileOutput : IJsonOutput
{
    // Use to keep track of the current filename
    // (one file per batch)
    private int _currentBatch = 1;

    // The filename that was passed in by the user
    private string _filename;

    // IJsonOutput properties
    public string ExpectedArgs { get; } = "--filename <OUTPUT>";
    public string Name { get; } = "file";

    // IJsonOutput methods
    public bool PreloadArgs(string[] args)
    {
        // Not necessary to use this library, but CommandLineUtils is setup
        // already to process the format passed in through 'args'.
        var subApp = new CommandLineApplication();

        // Marked as "single value" to indicate it should only appear once
        // in the input
        var filename = subApp.Option<string>("--filename|-f <file>", "The file template to write JSON data into",
            CommandOptionType.SingleValue);

        // This call ensures the argument is present, or an exception
        // is thrown
        filename.IsRequired();

        subApp.OnExecute(() => 
        { 
            // By this point all arguments have been parsed correctly, 
            // so record the value
            _filename = Path.GetFileNameWithoutExtensio(filename.ParsedValue); 
        });

        //This triggers the argument parsing and 'OnExecute' callback
        return subApp.Execute(args) == 0;
    }

    public Task WriteBatchAsync(IEnumerable<IDictionary<string, object>> json)
    {
        // One file per batch
        var filename = $"{_filename}_{_currentBatch++}.json";

        // Make both the serialization and write to file async
        return Task.Factory.StartNew(() =>
        {
            var serialized = JsonConvert.SerializeObject(json, Formatting.Indented);
            File.WriteAllText(filename, serialized, Encoding.UTF8);
        });
    }
}
```

There are two properties and two methods to implement in the interface.  The properties are

- Name: Used for registering the output type.  This will be used to find the type via the `--input` argument on the application.
- ExpectedArgs: Used for the `--list-output` argument of the program to let the user know what arguments to pass

The methods are:

- PreloadArgs: Read and parse the provided arguments.  This allows quick failure in case invalid arguments are given.
- WriteBatchAsync: The bulk of the logic will be here.  This will be HTTP calls, database writes, etc.  They should be asynchronous.