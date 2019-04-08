# Adding Generators

The generator types are the moustache formatted values inside the template JSON which dictate how the values will be created in the output JSON.  A quick example is `{{bool()}}` which will randomly output either `true` or `false`.  The interface to implement for this is [IDataGenerator](LargeDatasetGenerator.Abstractions/IDataGenerator.cs).  Your plugin will reference that assembly, compile, and then the executing program will reference your plugin.  Here is a walkthrough of a fairly simply generator that is included in the core library:

```c#
 public sealed class IntegerGenerator : IDataGenerator
{
    private readonly long _max;
    private readonly long _min;

    // IDataGenerator properties
    public string Description { get; } = "Generates random 64-bit integers between min and max";

    public string Signature { get; } = "{{integer(min: int64 = int64.min / 2, max: int64 = int64.max / 2)}}";

    public IntegerGenerator(string input)
    {
        // This constructor is optional in general, but required for
        // generators that accept arguments

        // See link below for this class
        var parser = new IntegerGeneratorArgs(input);
        var (min, max) = parser.Parse();
        _min = min;
        _max = max;
    }

    public IntegerGenerator()
    {
        // This constructor is not used in general, but rather for the
        // --list-generators mode so its functionality can be arbitrary
        var parser = new IntegerGeneratorArgs("()");
        var (min, max) = parser.Parse();
        _min = min;
        _max = max;
    }

    public Task<object> GenerateValueAsync()
    {
        // ThreadSafeRandom is just a locked wrapper around a static Random
        // instance and NextInt64 is an extension method for creating
        // 64-bit random numbers.
        return Task.FromResult<object>(ThreadSafeRandom.NextInt64(_min, _max));
    }
}
```

The `IntegerGeneratorArgs` class is somewhat large and complex looking so it is not included above but you can find it [here](LargeDatasetGenerator.Core/Generator/IntegerGenerator.cs).  It makes use of the [csly](https://github.com/b3b00/csly) lexer/parser library to parse the arguments out of the string.

There are two properties and one method to implement in the interface:

- Description: Used for the `--list-generators` argument of the program to give an indication of what the generator does.
- Signature: Also used for the `--list-generators` argument to show the arguments used.  Optional arguments will have `= <default value>` after them.
- GenerateValueAsync: Generates the next value for use in the output JSON.  Most are non-async and will return `Task.FromResult` but for future-proofing the ability to run asynchronously was used (random numbers from an Internet hardware source?)