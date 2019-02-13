#!/bin/bash

TOP="$( cd "$(dirname "$0")" ; pwd -P )/.."
pushd $TOP

CMAKE_DIRECTORY=$1
if [[ -z $CMAKE_DIRECTORY ]]; then
    echo "Error: No CMakeLists.txt directory specified, aborting..."
    exit 1
fi

./build.sh -p cbl-logtest -c Release -n -o "ci/cbl-log/build" -d $CMAKE_DIRECTORY
./build.sh -p cbl-log -c Release -n -d $CMAKE_DIRECTORY

pushd cbl-log/build

make install

popd

if [[ ! -d $TOP/install ]]; then
    mkdir -p $TOP/install
fi

pushd $TOP/install
rm -rf libicu* pkgconfig/ icu/
popd

popd


