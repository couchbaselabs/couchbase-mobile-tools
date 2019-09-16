# Couchbase Lite Error generation tool

This little script generates error messages for the various platform implementation of CBLite
from a single JSON source.

## Use
python3 gen_errors <path to JSON error file> <path to writeable output directory>

The official source of error message truth is cbl-errors.json.

## Source
cbl-errors.json  should be an easy to follow the template. The file contains a list of
objects, each of which is has a key (error) and a value (the message).  The keys are
used as variable names in some of the generated files.  Don't use keys that will cause
one of the compilers to barf.

The source supports template strings.  Templates are represented as a small integer
surrounded by curly braces: '{n}'. The presumption is that, within a message, distinct
values of n will be replaced by distinct texts and that identical values of n will be
replaced by identical strings.

Platforms code is on its own assuring that it provided the right number of arguments
to any error message template, assuring that they are appropriate, and for handling
any errors that arise from formatting.

## Generation
The code generation for each platfom is handled by a separate python module.  It should
be possible to change the generation for any one platform without affecting any other.

