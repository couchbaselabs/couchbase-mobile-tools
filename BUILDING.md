# Building the `cblite` tool

Building, for the most part, should be a fairly painless endeavor.  There are two scripts at the root of the repo -- `build.sh` for Linux and `build.ps1` for Windows -- and an Xcode project for macOS in the `Xcode` subdirectory.

(In this day and age shell scripts can be used on Windows and powershell can be used on Unix, but these scripts are platform specific, to avoid annoyances with directory separate chars).

The build script will automatically download LiteCore the first time it runs. You can configure which branch to download, with a `-b` flag.

The result will be `build/cblite`, a standalone executable that you can move wherever you like.

### Windows

The trick with Windows is that the location of things is less well defined than on Unix based systems.  The build script assumes that both CMake and Git are in the 'Program Files' directory.  If they are not those paths can be overriden with the `GitPath` and `CMakePath` arguments.

### macOS

1. If the `vendor/couchbase-lite-core` submoduel wasn't already checked out,  run `git submodule update --init --recursive`.
2. Open `cblite.xcodeproj` in the `Xcode` subdirectory.
3. Choose Product>Build.

Alternatively, you can install CMake (e.g. via HomeBrew) and run `build.sh`.

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

#### CentOS / RedHat

CentOS is tough to build for since the software available to it is so incredibly old.  For convenience, here is a docker container that creates an environment, and then builds the cblite tool in CentOS:

```
FROM centos:latest

RUN rpm -i https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
RUN yum install -y --setopt=keepcache=0 which svn git wget make centos-release-scl && yum install -y libicu-devel zlib-devel llvm-toolset-7
RUN cd /usr/local && wget https://cmake.org/files/v3.6/cmake-3.6.0-Linux-x86_64.sh && chmod 755 cmake-3.6.0-Linux-x86_64.sh && (echo y ; echo n) | sh ./cmake-3.6.0-Linux-x86_64.sh --prefix=/usr/local

RUN svn co http://llvm.org/svn/llvm-project/libcxx/trunk libcxx && \
cd libcxx && \
mkdir tmp && \
cd tmp && \
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_C_COMPILER=/opt/rh/llvm-toolset-7/root/usr/bin/clang -DCMAKE_CXX_COMPILER=/opt/rh/llvm-toolset-7/root/usr/bin/clang++ .. && \
make -j8 install && \
cd .. && \
rm tmp -rf && \
cd ..

RUN svn co http://llvm.org/svn/llvm-project/libcxxabi/trunk libcxxabi && \
cd libcxxabi && \
mkdir tmp && \
cd tmp && \
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_C_COMPILER=/opt/rh/llvm-toolset-7/root/usr/bin/clang -DCMAKE_CXX_COMPILER=/opt/rh/llvm-toolset-7/root/usr/bin/clang++ -DLIBCXXABI_LIBCXX_INCLUDES=../../libcxx/include .. && \
make -j8 install && \
cd ../..

RUN cd libcxx && \
mkdir tmp && \
cd tmp && \
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_C_COMPILER=/opt/rh/llvm-toolset-7/root/usr/bin/clang -DCMAKE_CXX_COMPILER=/opt/rh/llvm-toolset-7/root/usr/bin/clang++ -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_CXX_ABI_INCLUDE_PATHS=../../libcxxabi/include .. && \
make -j8 install

RUN git clone https://github.com/couchbaselabs/cblite
RUN cd cblite && CC=/opt/rh/llvm-toolset-7/root/usr/bin/clang CXX=/opt/rh/llvm-toolset-7/root/usr/bin/clang++ ./build.sh

ENV LD_LIBRARY_PATH /usr/lib
```
