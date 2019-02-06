#!/bin/bash

TOP=`dirname "$0"`
pushd $TOP

./build.sh -p cbl-logtest -c RelWithDebInfo -n
./build.sh -p cbl-log -c RelWithDebInfo -n

popd


