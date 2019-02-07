#!/bin/bash

TOP="$( cd "$(dirname "$0")" ; pwd -P )/.."
pushd $TOP

./build.sh -p cbl-logtest -c RelWithDebInfo -n -o "ci/cbl-log/build"
./build.sh -p cbl-log -c RelWithDebInfo -n

popd


