# The Couchbase Mobile Tool Repo

This repo is a collection of tools developed by the Couchbase mobile team.  Official support is not guaranteed by presence in this repo, and each tool gains official support (i.e. paid support help, etc) on a case-by-case basis.  For more information, open one of the following pages:

- [cblite](README.cblite.md)
- [cbl-log](README.cbl-log.md)
- [LargeDatasetGenerator](LargeDatasetGenerator/README.md)

## Building the Tools

First: if you haven't already checked out submodules, run `git submodule update --init --recursive`.

To install the dependencies for the build script run `pip install -r requirements.txt`.

There is a python script at the root of the repo called `build.py` which if run with no arguments will guide you through the build process (caveat: It might not work on "X weird system").  At the end of any interactive run, the script will print the non-interactive invocation to the screen for future reference.  For a more advanced description of how to build without the script see [this doc](BUILDING.md)


### Compiler Requirements

The recommended configuration is clang and libc++.  Here is a table of versions:

**Compiler (Linux)**

| Name  | Minimum | Recommended |
|-------|---------|-------------|
| GCC   | 7       | 7 or higher |
| clang | 3.9*    | 5 or higher |
\* Requires libstdc++-7 or libc++ from clang 5 or higher

**Compiler (Windows)**

Compiler versions on Windows have recently taken a turn beginning with Visual Studio 2017.  However, it appears that compiler versions from here forward will be contained within a ten digit minor version range.  Visual Studio 2017 was released with MSVC 14.10.25008 and is currently at MSVC 14.16.27027 while Visual Studio 2019 was released with MSVC 14.20.27508.  For the purposes of these tools, anything 14.10 or above is fine.  The C++ runtime installer for end users has been combined into one big installer for all of version 14 which overwrites the previous one instead of an installer per runtime like it used to be, so that makes checking if it is installed in an installer a bit more of a hassle.  If you've installed VS 2017, you are good to go for compiling.  For distribution you will need to check the MSVC version and install the VS2015-2019 C++ runtime distributable if needed.
