#!/bin/bash -e

dotnet_ver=$1

usage() {
    echo "Usage: prepare_dotnet.sh DOTNET_VERSION"
    echo 
    echo "Prepares the .NET CLI environment for use."
    echo 
    echo "Arguments:"
    echo
    echo -e "\tDOTNET_VERSION\tThe version of the .NET SDK to use (e.g. 8.0) (required)"
}

if [ "$dotnet_ver" == "" ]; then
    usage
    exit 1;
fi

# Not installed on macOS by default
command -v flock > /dev/null || (echo "flock not installed, please install it first"; exit 1)

# This script is often run in parallel so let's only allow one to run at once
touch /tmp/dotnet_testing_lock
exec 4</tmp/dotnet_testing_lock
flock -w 180 4 || (echo "Failed to acquire file lock to prepare .NET, aborting..."; exit 1)

export DOTNET_ROOT=$HOME/.dotnet
script_file=$(mktemp dotnet-install.sh.XXXXXX)
curl -L https://dot.net/v1/dotnet-install.sh -o $script_file
chmod +x $script_file
$PWD/$script_file -c $dotnet_ver
rm $script_file

$HOME/.dotnet/dotnet tool install --global --add-source https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-eng/nuget/v3/index.json Microsoft.DotNet.XHarness.CLI --version "8.0.0-prerelease*"
$HOME/.dotnet/dotnet workload install maui
