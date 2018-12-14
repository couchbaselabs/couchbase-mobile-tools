#!/bin/bash

function print_help {
    echo "Usage: build.sh --branch <branch-name>"
    echo
    echo "ARGUMENTS"
    echo "--branch |-b       The branch to use for building the tool"
    echo "--config |-c       The config to build (Debug (default), Release, MinSizeRel, RelWithDebInfo)"
    echo "--product|-p      The product to build (cblite (default) or cbl-log)"
}

which git > /dev/null
if [ $? -ne 0 ]; then
    echo "git not found, aborting..."
    exit 2
fi

which cmake > /dev/null
if [ $? -ne 0 ]; then
    echo "cmake not found, aborting..."
    exit 3
fi

which make > /dev/null
if [ $? -ne 0 ]; then
    echo "mmake not found, aborting..."
    exit 4
fi

CONFIG="Debug"
BRANCH="master"
PRODUCT="cblite"
while (( "$#" )); do
  case "$1" in
    -b|--branch)
      BRANCH=$2
      shift 2
      ;;
    -c|--config)
      CONFIG=$2
      shift 2
      ;;
    -p|--product)
      PRODUCT=$2
      shift 2
      ;;
    --) # end argument parsing
      shift
      break
      ;;
    -*|--*=) # unsupported flags
      echo "Error: Unsupported flag $1" >&2
      print_help
      exit 1
      ;;
    *)
      echo "Error: Position arguments not allowed ($1 given)" >&2
      print_help
      exit 1
      ;;
  esac
done

if [[ ! -d vendor/couchbase-lite-core ]]; then
    git clone https://github.com/couchbase/couchbase-lite-core vendor/couchbase-lite-core
fi

git submodule update --init --recursive
pushd vendor/couchbase-lite-core
git reset --hard
git checkout $BRANCH
git fetch origin
git pull origin $BRANCH
git submodule update --init --recursive
popd

if [[ ! -d build ]]; then
    mkdir build
fi

pushd build
cmake -DCMAKE_BUILD_TYPE=$CONFIG ..
make -j8 $PRODUCT
popd
