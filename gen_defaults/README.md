# Default Value Source Generator

This python script will generate default values for C, Java, Objective-C, and Swift (C# coming soon) based on an input JSON value that will be described later.  There are no python packages needed, and no arguments.  Simply run `./gen_defaults.py` from this directory and the needed files for each language will be output into its own subfolder.

:note: Swift default generation is based on the values generated for Objective-C and so they must both be generated from the same JSON input.

## The Input Schema
The input is a JSON file that holds information about variables in an abstract format that is roughly described as follows:

```json
{
    "[ClassName]": {
        "constants": [
            {
                "name": "[variable name]",
                "value": "[scalar value, or complex]",
                "type": {
                    "id": "[type]",
                    "subset": "enum/system"
                },
                "description": "[information about the constant]"
            }
        ]
    }
}
```

Going through this bit by bit, it starts with a class name, which is just a concept for grouping together constants that apply in a similar area.  It doesn't always turn into an actual class, since not all languages support that.  For example, on C the structure turns into `kCBL[ClassName][variable name]`.  Information in this area may be expanded later, but for now each one of these entries contains a list of constants.

The constants each have a name, type, and decription.  The name and description are always strings and translate directly to the name of the variable that gets output (or the end of the variable as in the above C), and a comment above it respectively.  The type is slightly more complex in that it has a `subset` property that governs how the actual value will be output.  The majority of the constants will be the `system` subset which means that the `id` can be easily translated into an existing type provided by the output language.  Currently, the only other subset is `enum` which means that the type is treated as an enum defined either by the language runtime itself or some other library.  This has bearing on how the default value is output, since it is no longer something like a simple numeric or string value.

The last item is `value`, which in many cases can be a simple JSON scalar like `"foo"`, `42`, or `true`.  However in some cases, like a time interval, more context is needed.  In that case the value takes the following form:

```json
{
    "scalar": "[value]",
    "unit": "[context for value]"
}
```

In the example of a time interval, scalar would be the numeric value and unit would be the measurement such as seconds or milliseconds.  Currently only seconds are needed, and so only seconds are supported but this is easily extendable to other units, and other types of context-needed values.