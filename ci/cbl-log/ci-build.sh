#!/bin/bash

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
INSTALL_PREFIX=`cat CMakeCache.txt| grep CMAKE_INSTALL_PREFIX | cut -f 2 -d '='`

popd

if [[ ! -d $TOP/install ]]; then
    mkdir -p $TOP/install
fi

pushd $INSTALL_PREFIX/lib
echo $INSTALL_PREFIX/lib
rm -rf libicu* pkgconfig/ icu/
