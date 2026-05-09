#!/bin/bash -e

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

# Not installed on macOS by default
command -v flock > /dev/null || (echo "flock not installed, please install it first"; exit 1)
 
### BEGIN OF SCRIPT ###
# ========== END OF THIRD PARTY ===============================

BOLD=""
UNDERLING=""
STANDOUT=""
NORMAL=""
black=""
RED=""
GREEN=""
YELLOW=""
BLUE=""
MAGENTA=""
CYAN=""
WHITE=""
if test -t 1; then
    ncolors=$(tput colors)
    if test -n "$ncolors" && test $ncolors -ge 8; then
        BOLD="$(tput bold)"
        UNDERLING="$(tput smul)"
        STANDOUT="$(tput smso)"
        NORMAL="$(tput sgr0)"
        black="$(tput setaf 0)"
        RED="$(tput setaf 1)"
        GREEN="$(tput setaf 2)"
        YELLOW="$(tput setaf 3)"
        BLUE="$(tput setaf 4)"
        MAGENTA="$(tput setaf 5)"
        CYAN="$(tput setaf 6)"
        WHITE="$(tput setaf 7)"
    fi
fi


function banner() {
    echo
    echo ${GREEN}===== $1 =====${NORMAL}
    echo
}

function run_locked() {
    exlock_wait 180 || (echo "Failed to acquire file lock, aborting..."; exit 1)
    $@
    _no_more_locking
}

function _install_dotnet() {
    if [ -z "$DOTNET_ROOT" ]; then
        echo "Error: DOTNET_ROOT is not set."
        exit 1
    fi
    
    version=${1:-"8.0"}
    banner "Installing .NET SDK $version"

    script_file=$(mktemp dotnet-install.sh.XXXXXX)
    curl -L https://dot.net/v1/dotnet-install.sh -o $script_file
    chmod +x $script_file
    $PWD/$script_file -c $version --skip-non-versioned-files --install-dir $DOTNET_ROOT
    rm $script_file
}

function install_dotnet() {
    run_locked _install_dotnet $@
}

function _install_dotnet_runtime() {
    if [ -z "$DOTNET_ROOT" ]; then
        echo "Error: DOTNET_ROOT is not set."
        exit 1
    fi

    version=${1:-"8.0"}
    banner "Installing .NET Runtime $version"

    script_file=$(mktemp dotnet-install.sh.XXXXXX)
    curl -L https://dot.net/v1/dotnet-install.sh -o $script_file
    chmod +x $script_file
    $PWD/$script_file -c $version --runtime dotnet --skip-non-versioned-files --install-dir $DOTNET_ROOT
    rm $script_file
}

function install_dotnet_runtime() {
    run_locked _install_dotnet_runtime $@
}

function _install_xharness() {
    if [ -z "$DOTNET_ROOT" ]; then
        echo "Error: DOTNET_ROOT is not set."
        exit 1
    fi
    banner "Installing XHarness"
    $DOTNET_ROOT/dotnet tool install --global --add-source https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-eng/nuget/v3/index.json Microsoft.DotNet.XHarness.CLI --version "10.0.0-prerelease*"
}

function install_xharness() {
    run_locked _install_xharness
}
