# Building the `cblite` tool

Building, for the most part, should be a fairly painless endeavor.  There are two scripts at the root of the repo `build.sh` (for Unix) and `build.ps1` for Windows.  In this day and age shell script can be used on Windows and powershell can be used on Unix but these scripts are platform specific (to avoid annoyances with directory separate chars).  Each of them has help that you can print, but you must specify a branch of Couchbase Lite Core to use when building.  The script will automatically fetch the repo and branch for you.  

### Windows

The trick with Windows is that the location of things is less well defined than on Unix based systems.  The build script assumes that both CMake and Git are in the 'Program Files' directory.  If they are not those paths can be overriden with the `GitPath` and `CMakePath` arguments.

### macOS

There are no tricks here, the build script should work as is as long as you have cmake installed.

### Linux

Linux is a tricky situation because the only fully supported toolchain is clang / libc++ 3.9.1+.  That means that if it is not available on your system you will need to compile it or get it somehow.  Here is an example list of what you need dependency-wise to run the build script (based on Ubuntu Bionic):  `clang libc++-dev libc++abi-dev libicu-dev cmake make git`.  For running, you will only need to get `libc++ libc++abi libicu`.  `zlib` is another dependency but it usually is preinstalled on most distros.  Here is a list of dependencies via `ldd` for reference:

```
linux-vdso.so.1 (0x00007ffebd38e000)
	libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007f9253bbe000)
	libz.so.1 => /lib/x86_64-linux-gnu/libz.so.1 (0x00007f92539a1000)
	libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007f925379d000)
	libicuuc.so.60 => /usr/lib/x86_64-linux-gnu/libicuuc.so.60 (0x00007f92533e6000)
	libicui18n.so.60 => /usr/lib/x86_64-linux-gnu/libicui18n.so.60 (0x00007f9252f45000)
	libc++.so.1 => /usr/lib/x86_64-linux-gnu/libc++.so.1 (0x00007f9252c82000)
	libc++abi.so.1 => /usr/lib/x86_64-linux-gnu/libc++abi.so.1 (0x00007f9252a52000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f92526b4000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f925249c000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f92520ab000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f9253ddd000)
	libicudata.so.60 => /usr/lib/x86_64-linux-gnu/libicudata.so.60 (0x00007f9250502000)
	libstdc++.so.6 => /usr/lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f9250179000)
	librt.so.1 => /lib/x86_64-linux-gnu/librt.so.1 (0x00007f924ff71000)
  ```
