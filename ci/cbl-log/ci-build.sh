#!/bin/bash -e

TOP="$( cd "$(dirname "$0")" ; pwd -P )/../.."
pushd $TOP

mkdir -p ci/cbl-log/build
pushd ci/cbl-log/build
cmake -DCMAKE_BUILD_TYPE=Release $TOP/cbl-log
make -j$(nproc) cbl-log
make -j$(nproc) cbl-logtest

make install
INSTALL_PREFIX=`cmake -L -S $TOP/cbl-log | grep ^CMAKE_INSTALL_PREFIX | cut -f 2 -d '='`
if [[ "$INSTALL_PREFIX" == "/" ]] || [[ "$INSTALL_PREFIX" == "" ]]; then
    echo "Refusing to proceed at root of filesystem"
    exit 1
fi

popd