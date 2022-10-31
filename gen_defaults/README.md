# Default Value Source Generator

This python script will generate default values for C, Java, Objective-C, and Swift based on an input JSON value that will be described later.  There are no python packages needed, and no arguments.  Simply run `./gen_defaults.py` from this directory and the needed files for each language will be output into its own subfolder.

:note: Swift default generation is based on the values generated for Objective-C and so they must both be generated from the same JSON input.

## The Input Schema
The input is a JSON file that holds information about variables in an abstract format that is roughly described as follows:

```json
{
    "[ClassName]": {
        "ee": true,
        "only_on": "[platform, list]",
        "long_name": "[long class name]",
        "constants": [
            {
                "name": "[variable name]",
                "references": "[other variable name]",
                "value": "[scalar value, or complex]",
                "type": {
                    "id": "[type]",
                    "subset": "enum/system"
                },
                "description": "[information about the constant]",,
                "only_on": "[platform, list]"
            }
        ]
    }
}
```

Going through this bit by bit, it starts with a class name, which is just a concept for grouping together constants that apply in a similar area.  It doesn't always turn into an actual class, since not all languages support that.  For example, on C the structure turns into `kCBL[ClassName][variable name]`.  The class name contains a long name, optional list of languages (i.e. it will only be generated for those languages), an ee (enterprise edition) boolean indicating that it is only applicable to the enterprise edition, and a list of constants.  The long name is for implementations that wish to refer back to the class that these default values apply to (e.g. Listener applies to URLEndpointListener)

The constants each have a name, type, decription, and an optional list of languages which functions the same as in the class name.  The name and description are always strings and translate directly to the name of the variable that gets output (or the end of the variable as in the above C), and a comment above it respectively.  The type is slightly more complex in that it has a `subset` property that governs how the actual value will be output.  The majority of the constants will be the `system` subset which means that the `id` can be easily translated into an existing type provided by the output language.  Currently, the only other subset is `enum` which means that the type is treated as an enum defined either by the language runtime itself or some other library.  This has bearing on how the default value is output, since it is no longer something like a simple numeric or string value.

A notable addition to the previous paragraph is the language specific type key.  For example, `type_objc` will override the `type` value for the Objective-C language.  This should be used sparingly and only as a means to accomodate existing discrepencies in API types.  

The constant can optionally have a `references` value to indicate which variable on its target class that it references.  By default this will be the same as the `name` value.

The last item is `value`, which in many cases can be a simple JSON scalar like `"foo"`, `42`, or `true`.  However in some cases, like a time interval, more context is needed.  In that case the value takes the following form:

```json
{
    "scalar": "[value]",
    "unit": "[context for value]"
}
```

In the example of a time interval, scalar would be the numeric value and unit would be the measurement such as seconds or milliseconds.  Currently only seconds are needed, and so only seconds are supported but this is easily extendable to other units, and other types of context-needed values.