#!/bin/bash -e

TOP="$( cd "$(dirname "$0")" ; pwd -P )/../.."
pushd $TOP

CMAKE_DIRECTORY=$1
if [[ -z $CMAKE_DIRECTORY ]]; then
    echo "Error: No CMakeLists.txt directory specified, aborting..."
    exit 1
fi

mkdir -p ci/cbl-log/build
pushd ci/cbl-log/build
cmake -DCMAKE_BUILD_TYPE=Release $CMAKE_DIRECTORY
make -j8 cbl-log
make -j8 cbl-logtest

make install
INSTALL_PREFIX=`cmake -L -S $CMAKE_DIRECTORY | grep ^CMAKE_INSTALL_PREFIX | cut -f 2 -d '='`
if [[ "$INSTALL_PREFIX" == "/" ]] || [[ "$INSTALL_PREFIX" == "" ]]; then
    echo "Refusing to proceed at root of filesystem"
    exit 1
fi

popd

if [[ ! -d $TOP/install ]]; then
    mkdir -p $TOP/install
fi

pushd $INSTALL_PREFIX/lib
rm -r * # Normal deps of LiteCore don't apply to cbl-log

if [ `uname -s` != Linux ]; then
    exit 0
fi

# Get libstdc++ in the package
libstdcpp=`g++ --print-file-name=libstdc++.so`
libstdcpp=`realpath -s $libstdcpp`
libstdcppname=`basename "$libstdcpp"`

echo "-- Copying $libstdcpp to $libstdcppname..."
cp -p "$libstdcpp" "$libstdcppname"

echo "-- Linking ${libstdcppname}.6 to $libstdcppname..."
ln -s "$libstdcppname" "${libstdcppname}.6"
