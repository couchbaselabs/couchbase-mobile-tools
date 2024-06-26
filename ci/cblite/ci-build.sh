#!/bin/bash -e

TOP="$( cd "$(dirname "$0")" ; pwd -P )/../.."
pushd $TOP

CMAKE_DIRECTORY=$1
if [[ -z $CMAKE_DIRECTORY ]]; then
    echo "Error: No CMakeLists.txt directory specified, aborting..."
    exit 1
fi

mkdir -p ci/cblite/build
pushd ci/cblite/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_ENTERPRISE=ON $CMAKE_DIRECTORY
make -j8 cblite
make -j8 cblitetest

make install
INSTALL_PREFIX=`cmake -L $CMAKE_DIRECTORY | grep ^CMAKE_INSTALL_PREFIX | cut -f 2 -d '='`
if [[ "$INSTALL_PREFIX" == "/" ]] || [[ "$INSTALL_PREFIX" == "" ]]; then
    echo "Refusing to proceed at root of filesystem"
    exit 1
fi

popd

if [[ ! -d $TOP/install ]]; then
    mkdir -p $TOP/install
fi

pushd $INSTALL_PREFIX/lib
echo $INSTALL_PREFIX/lib
rm -rf pkgconfig/ libLiteCore.*

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
