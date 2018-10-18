#!/bin/bash

function print_help {
    echo "Usage: build.sh --branch <branch-name>"
    echo
    echo "ARGUMENTS"
    echo "--branch|-b       The branch to use for building the tool"
}

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

