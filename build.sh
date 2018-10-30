#!/bin/bash

function print_help {
    echo "Usage: build.sh --branch <branch-name>"
    echo
    echo "ARGUMENTS"
    echo "--branch|-b       The branch to use for building the tool"
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

while (( "$#" )); do
  case "$1" in
    -b|--branch)
      BRANCH=$2
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

pushd vendor/couchbase-lite-core
git reset --hard
git checkout $BRANCH
git pull origin
git submodule update --init --recursive
popd

if [[ ! -d build ]]; then
    mkdir build
fi

pushd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8 cblite
popd
