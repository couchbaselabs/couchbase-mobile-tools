#!/bin/bash -e

TOP="$( cd "$(dirname "$0")" ; pwd -P )/../.."
pushd $TOP

mkdir -p ci/cblite/build
pushd ci/cblite/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_ENTERPRISE=ON ${TOP}/cblite
make -j$(nproc) cblite
make -j$(nproc) cblitetest

make install
INSTALL_PREFIX=`cmake -L -S ${TOP}/cblite | grep ^CMAKE_INSTALL_PREFIX | cut -f 2 -d '='`
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