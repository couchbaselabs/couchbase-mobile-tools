# Guide to Building

The first thing you should try is the interactive python script `build.py` (which will tell you the non-interactive command at the end of its run).  If you are here it is because it is either not working or you are extra curious.  Each app in here is different (you can think of this repo as containing, basically, several repos in one) and they are organized into their own folders.

## cbl-log

This is a distributed and officially licensed product.  It is a CMake project, in the `cbl-log` folder.  It contains two targets:

- `cbl-log` : The final executable
- `cbl-logtest` : The test harness for cbl-log

The product is buildable via standard CMake commands, so the same as any other CMake based solution you've come across.  There are no CMake options to worry about. For example:

    $ cd cbl-log
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j 5

## cblite

This is a command line tool for inspecting, querying, and replicating Couchbase Lite databases, somewhat like the `sqlite3` tool.  It is *not* an officially licensed product (open source license only), but provided for convenience.  It is a CMake project, in the `cblite` folder.  It contains two targets:

- `cblite` : The final executable
- `cblitetest` : The test harness for cblite

The product is buildable via standard CMake commands, so the same as any other CMake based solution you've come across.  There are no CMake options to worry about. For example:

    $ cd cblite
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j 5

Or, on macOS, you can open the Xcode project in the `Xcode` directory and build the `cblite` scheme. (Remember that, to build an optimized/release version, you'll need to do **Product>Archive** or **Product>Build For>Profiling**.)

## LargeDatasetGenerator

This is a tool for generating a large set of proceduraly generated data to be inserted into various endpoints.  It can be useful for load testing various components.  It is *not* an officially licensed product (open source license only).  

It is a dotnet CLI project, which means to build it you will need to install the [.NET Core SDK](https://dotnet.microsoft.com/download).  After that, the CLI is pretty similar to any other build system.  `dotnet build` builds, `dotnet build -c Release` builds Release, etc.  If you want to build the program without any dependencies on the .NET runtime (native dependencies of the runtime itself still required, beware), then follow the guide for [.NET Core 2.x](https://dotnetthoughts.net/how-to-create-a-self-contained-dotnet-core-application/) (good) or [.NET Core 3.x](https://gunnarpeipman.com/dotnet-core-self-contained-executable/) (much better.)
