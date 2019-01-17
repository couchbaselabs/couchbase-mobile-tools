#!/bin/bash

TOP=`dirname "$0"`
BRANCH=""
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

pushd $TOP

if [[ ! -z $BRANCH ]]; then
    ./build.sh -p cbl-logtest -b $BRANCH -c RelWithDebInfo
    ./build.sh -p cbl-log -b $BRANCH -c RelWithDebInfo
else
    ./build.sh -p cbl-logtest -c RelWithDebInfo
    ./build.sh -p cbl-log -c RelWithDebInfo
fi

popd


