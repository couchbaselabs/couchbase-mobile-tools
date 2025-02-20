#!/bin/bash -e

TOP="$( cd "$(dirname "$0")" ; pwd -P )/../.."
pushd $TOP

if [[ -z $WORKSPACE ]]; then
    echo "Error: WORKSPACE envvar not specified, aborting..."
    exit 1
fi

mkdir -p ci/cbl-log/build
pushd ci/cbl-log/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$WORKSPACE/install" $TOP/cbl-log
make -j$(nproc) cbl-log
make -j$(nproc) cbl-logtest

make install