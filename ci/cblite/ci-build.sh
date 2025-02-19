#!/bin/bash -e

TOP="$( cd "$(dirname "$0")" ; pwd -P )/../.."
pushd $TOP

if [[ -z $WORKSPACE ]]; then
    echo "Error: WORKSPACE envvar not specified, aborting..."
    exit 1
fi

mkdir -p ci/cblite/build
pushd ci/cblite/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$WORKSPACE/install" -DBUILD_ENTERPRISE=ON ${TOP}/cblite
make -j$(nproc) cblite
make -j$(nproc) cblitetest

make install