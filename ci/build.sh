#!/bin/bash

function print_help {
    echo "Usage: build.sh --branch <branch-name>"
    echo
    echo "ARGUMENTS"
    echo "--branch |-b       The branch to use for building the tool"
    echo "--config |-c       The config to build (Debug (default), Release, MinSizeRel, RelWithDebInfo)"
    echo "--product|-p       The product to build (cblite (default) or cbl-log)"
    echo "--output|-o        Defines where the output of the build should go (default ci/<product>/build)"
    echo "--no-submodule|-n  Don't pull any submodules (if using another repo management tool)"
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

TOP=`dirname "$0"`/..
pushd $TOP

CONFIG="Debug"
BRANCH=""
PRODUCT="cblite"
NO_SUBMODULE=false
OUTPUT=""
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
    -n|--no-submodule)
      NO_SUBMODULE=true
      shift 1
      ;;
    -o|--output)
      OUTPUT=$2
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

if [[ -z $OUTPUT ]]; then
    OUTPUT="ci/$PRODUCT/build"
fi

if $NO_SUBMODULE; then
    echo "Skipping submodule checkout..."
else
    git submodule update --init --recursive
fi

if [[ ! $NO_SUBMODULE && ! -z $BRANCH ]]; then
    echo "Checking out branch $BRANCH of LiteCore..."
    pushd vendor/couchbase-lite-core
    git reset --hard
    git checkout $BRANCH
    git fetch origin
    git pull origin $BRANCH
    git submodule update --init --recursive
    popd
fi

if [[ ! -d $OUTPUT ]]; then
    mkdir $OUTPUT
fi

pushd $OUTPUT
cmake -DCMAKE_BUILD_TYPE=$CONFIG ../../..
make -j8 $PRODUCT
popd
popd
