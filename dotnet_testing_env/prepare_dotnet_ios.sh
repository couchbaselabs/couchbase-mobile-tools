#!/bin/bash -e

usage() {
    echo "Usage: prepare_dotnet_ios.sh DOTNET_VERSION XCODE_VERSION [OPTIONS]"
    echo 
    echo "Prepares the .NET CLI environment for use with iOS."
    echo 
    echo "Arguments:"
    echo
    echo -e "\tDOTNET_VERSION\t\tThe version of the .NET SDK to use (e.g. 8.0) (required)"
    echo -e "\tXCODE_VERSION\t\tThe version of Xcode to use for creating simulators (e.g. 15.2.0) (required)"
    echo -e "\t--simulator-device, -d VALUE"
    echo -e "\t\t\t\tThe device to use to create the simulator, if needed"
    echo -e "\t\t\t\t  Searches for the newest model if not provided"
    echo -e "\t\t\t\t  Example: iPhone 15 Pro Max"
    echo -e "\t--simulator-runtime, -r VALUE"
    echo -e "\t\t\t\tThe runtime to use to create the simulator, if needed"
    echo -e "\t\t\t\t  Searches for the newest if not provided"
    echo -e "\t\t\t\t  Example: 17.2"
    echo -e "\t--no-dotnet\t\tSkip the .NET SDK setup step (DOTNET_VERSION not required)"
}

# Pre-scan for --no-dotnet to determine whether DOTNET_VERSION is required
skip_dotnet=0
for arg in "$@"; do
    if [ "$arg" = "--no-dotnet" ]; then
        skip_dotnet=1
        break
    fi
done

dotnet_ver=""
xcode_ver=""

if [ $skip_dotnet -eq 0 ] && [[ $# -gt 0 && "$1" != -* ]]; then
    dotnet_ver=$1
    dotnet_major=$(echo $dotnet_ver | cut -d. -f1)
    shift
fi

if [[ $# -gt 0 && "$1" != -* ]]; then
    xcode_ver=$1
    shift
fi

export DEVELOPER_DIR=/Applications/Xcode-$xcode_ver.app
if [ $skip_dotnet -eq 0 ]; then
    export DOTNET_ROOT=$HOME/.dotnet$dotnet_major
fi

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--simulator-device)
            input_sim_device=$2
            simulator_device="com.apple.CoreSimulator.SimDeviceType.$(echo $2 | tr ' ' -)"
            shift
            shift
            ;;
        -d=*|--simulator-device=*)
            input_sim_device=${1#*=}
            simulator_device="com.apple.CoreSimulator.SimDeviceType.$(echo ${1#*=} | tr ' ' -)"
            shift
            ;;
        -r|--simulator-runtime)
            input_sim_runtime="iOS $2"
            simulator_runtime="com.apple.CoreSimulator.SimRuntime.iOS-$(echo $2 | tr . -)"
            shift
            shift
            ;;
        -r=*|--simulator-runtime=*)
            input_sim_runtime="iOS ${1#*=}"
            simulator_runtime="com.apple.CoreSimulator.SimRuntime.iOS-$(echo ${1#*=} | tr . -)"
            shift
            ;;
        --no-dotnet)
            shift
            ;;
        *)
            echo "Unknown argument $1"
            exit 1
            ;;
    esac
done

if [ $skip_dotnet -eq 0 ] && [ "$dotnet_ver" = "" ]; then
    usage
    exit 1
fi

if [ "$xcode_ver" = "" ]; then
    usage
    exit 1
fi

if [ "$simulator_device" = "" ]; then
    input_sim_device=$(xcrun simctl list -j | jq '.devicetypes[] | select(.name | contains("iPhone")) | .name' | tail -1 | tr -d '"')
    simulator_device=$(xcrun simctl list -j | jq '.devicetypes[] | select(.name | contains("iPhone")) | .identifier' | tail -1 | tr -d '"')
fi

if [ "$simulator_runtime" = "" ]; then
    input_sim_runtime=$(xcrun simctl list -j | jq '.runtimes[] | .name' | tail -1 | tr -d '"')
    simulator_runtime=$(xcrun simctl list -j | jq '.runtimes[] | .identifier' | tail -1 | tr -d '"')
fi

existing_sim=$((xcrun simctl list | grep dotnet_cbl_testing) || true)
if [[ "$existing_sim" = "" ]]; then
	echo "dotnet_cbl_testing not found, creating $input_sim_device ($input_sim_runtime) sim..."
    xcrun simctl create dotnet_cbl_testing $simulator_device $simulator_runtime
fi

boot_state=$(xcrun simctl list | grep dotnet_cbl_testing | awk '{print $3}' | tr -d '()')
if [ "$boot_state" = "Shutdown" ]; then
    echo "dotnet_cbl_testing is shutdown, booting now..."
    xcrun simctl boot dotnet_cbl_testing
fi

open -a simulator
if [ $skip_dotnet -eq 0 ]; then
    echo "Setting up .NET $dotnet_ver..."
    source ./prepare_dotnet.sh
    install_dotnet $dotnet_ver
    install_xharness
fi
