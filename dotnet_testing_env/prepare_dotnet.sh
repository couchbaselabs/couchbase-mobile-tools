#!/bin/bash -e

# Not installed on macOS by default
command -v flock > /dev/null || (echo "flock not installed, please install it first"; exit 1)
(return 0 2>/dev/null) && sourced=1 || sourced=0
if [ $sourced -eq 1 ]; then
    echo "WARNING: This script uses a file lock and was sourced"
    echo "which means that the calling script will also block"
    echo "any other scripts which call this one!  This is not"
    echo "recommended"
fi

# ========== THIRD PARTY =====================================
# SPDX-License-Identifier: MIT
 
## Copyright (C) 2009 Przemyslaw Pawelczyk <przemoc@gmail.com>
##
## This script is licensed under the terms of the MIT license.
## https://opensource.org/licenses/MIT
#
# Lockable script boilerplate
 
### HEADER ###
 
LOCKFILE="/tmp/dotnet_testing_lock"
LOCKFD=99
 
# PRIVATE
_lock_wait()        { flock -w $1 $LOCKFD; }
_lock()             { flock -$1 $LOCKFD; }
_no_more_locking()  { _lock u; }
_prepare_locking()  { eval "exec $LOCKFD>\"$LOCKFILE\""; trap _no_more_locking EXIT; }
 
# ON START
_prepare_locking
 
# PUBLIC
exlock_wait()        { _lock_wait $1; }  # obtain an exclusive lock within the time limit or fail
 
### BEGIN OF SCRIPT ###
# ========== END OF THIRD PARTY ===============================
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

# This script is often run in parallel so let's only allow one to run at once
exlock_wait 180 || (echo "Failed to acquire file lock to prepare .NET, aborting..."; exit 1)

script_file=$(mktemp dotnet-install.sh.XXXXXX)
curl -L https://dot.net/v1/dotnet-install.sh -o $script_file
chmod +x $script_file
$PWD/$script_file -c $dotnet_ver
rm $script_file

$HOME/.dotnet/dotnet tool install --global --add-source https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-eng/nuget/v3/index.json Microsoft.DotNet.XHarness.CLI --version "8.0.0-prerelease*"
$HOME/.dotnet/dotnet workload install maui
