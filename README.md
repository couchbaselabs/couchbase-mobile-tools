# The Couchbase Mobile Tool Repo

> **Master branch:** This branch tracks the master branch of couchbase-lite-core, which is currently development versions of Couchbase Lite 2.8. For the Couchbase Lite 2.7 versions, check out the `master-mercury` branch.

This repo is a collection of tools developed by the Couchbase mobile team.  Official support is not guaranteed by presence in this repo, and each tool gains official support (i.e. paid support help, etc) on a case-by-case basis.  For more information, open one of the following pages:

- [cblite](README.cblite.md)
- [cbl-log](README.cbl-log.md)
- [LargeDatasetGenerator](LargeDatasetGenerator/README.md)

## Binary Releases

For convenience we've uploaded some pre-built binaries to the [Releases](https://github.com/couchbaselabs/couchbase-mobile-tools/releases) tab. As these tools are unsupported, the binaries may or may not be up to date.

> NOTE: The source code for the tools is Apache-licensed, as specified in [LICENSE](https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/LICENSE). However, the pre-built [cblite](https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/README.cblite.md) binaries are linked with the Enterprise Edition of Couchbase Lite, so the usage of those binaries will be guided by the terms and conditions specified in Couchbase's [Enterprise License](https://www.couchbase.com/ESLA01162020).

## Building the Tools

First: if you haven't already checked out submodules, run `git submodule update --init --recursive`.

### Interactively

There is a Python script at the root of the repo called `build.py` which if run with no arguments will guide you through the build process.

To install the dependencies for the build script, run `pip install -r requirements.txt`. (Or use `pip3` if you don't have `pip`.)

Then run `python build.py` (or `python3 build.py`) to build.

 At the end of any interactive run, the script will print the non-interactive invocation to the screen for future reference. In the future you could just re-enter that command, if you like.

### With CMake or Xcode

For a description of how to build without the script, see [BUILDING.md](BUILDING.md)

### Compiler Requirements

#### Linux

The recommended configuration is clang and libc++.  Here is a table of versions:

| Name  | Minimum | Recommended |
|-------|---------|-------------|
| GCC   | 7       | 7 or higher |
| Clang | 3.9\*    | 5 or higher |

\* Requires libstdc++-7 or libc++ from clang 5 or higher

#### Windows

Compiler versions on Windows have recently taken a turn beginning with Visual Studio 2017.  However, it appears that compiler versions from here forward will be contained within a ten digit minor version range.  Visual Studio 2017 was released with MSVC 14.10.25008 and is currently at MSVC 14.16.27027 while Visual Studio 2019 was released with MSVC 14.20.27508.  

For the purposes of these tools, anything 14.10 or above is fine.  

The C++ runtime installer for end users has been combined into one big installer for all of version 14 which overwrites the previous one instead of an installer per runtime like it used to be, so that makes checking if it is installed in an installer a bit more of a hassle.  

If you've installed VS 2017, you are good to go for compiling.  For distribution you will need to check the MSVC version and install the VS2015-2019 C++ runtime distributable if needed.

#### macOS

Xcode 11.4 or later recommended.
