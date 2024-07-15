#!/bin/bash -e

EMULATOR_BIN=$(dirname $(readlink -f $(which avdmanager)))/../../../emulator/emulator
ADB_BIN=$(dirname $(readlink -f $(which avdmanager)))/../../../platform-tools/adb
export JAVA_HOME=`/usr/libexec/java_home -v 17.0`

usage() {
    echo "Usage: prepare_dotnet_android.sh DOTNET_VERSION [OPTIONS]"
    echo 
    echo "Prepares the .NET CLI environment for use with Android."
    echo 
    echo "Arguments:"
    echo
    echo -e "\tDOTNET_VERSION\t\tThe version of the .NET SDK to use (e.g. 8.0) (required)"
    echo -e "\t--emulator-api-level, -l VALUE"
    echo -e "\t\t\t\tThe API level to use to create the emulator, if needed (default 22)"
}

find_compatible_device() {
    echo "Looking for emulator running AVD 'dotnet_cbl_testing_$1'" >&2
    for device in $($ADB_BIN devices | grep -v "List" | awk '{print $1}'); do
        echo "Examining Android device '$device'..." >&2
        found_name=$($ADB_BIN -s $device shell getprop ro.kernel.qemu.avd_name | tr -d '\r') # Thanks for the useless carriage return adb...
        if [ "$found_name" == "dotnet_cbl_testing_$1" ]; then
            echo "Found a match!" >&2
            echo -n $device
            return
        else 
            echo "Found $found_name, not a match" >&2
        fi
   done
}

create_avd() {
    existing_avd=$((avdmanager list avd | grep dotnet_cbl_testing_$1) || true)
    if [[ "$existing_avd" == "" ]]; then
        echo "dotnet_cbl_testing_$1 AVD not found, creating Android API $1 $2 AVD..."
        sdkmanager --install "system-images;android-$1;default;$2" "platform-tools" "platforms;android-34"
        avdmanager -s create avd -n dotnet_cbl_testing_$1 -k "system-images;android-$1;default;$2" -c 1024M
    else
        echo "Found existing AVD dotnet_cbl_testing_$1, skipping create"
    fi
}

launch_emulator() {
    if [ ! -f $EMULATOR_BIN ]; then
        echo "emulator not installed!  This script will not install this automatically"
        echo "because it is not versioned, and at the time of writing the latest version"
        echo "crashes a lot on Apple Silicon.  Please install it before running this script"
        exit 1
    fi

    EMULATOR_BIN=$(realpath $EMULATOR_BIN)
    $EMULATOR_BIN @dotnet_cbl_testing_$1 -memory 1024 -no-snapshot -netspeed full -netdelay none -no-boot-anim &
}

case $(uname -m) in
    x86_64) android_arch="x86_64";;
    aarch64) android_arch="arm64-v8a";;
    arm64) android_arch="arm64-v8a";;
    *) echo "Invalid architecture $ARCH, aborting..."; exit 5;;
esac

dotnet_ver=$1
if [ "$dotnet_ver" == "" ]; then
    usage
    exit 1
fi

emulator_api_level=22

shift # Get to the optionals
while [[ $# -gt 0 ]]; do
    case $1 in
        -l|--emulator-api-level)
            emulator_api_level=$2
            shift
            shift
            ;;
        -l=*|--emulator-api-level=*)
            emulator_api_level=${1#*=}
            shift
            ;;
        *)
            echo "Unknown argument $1"
            exit 1
            ;;
    esac
done

usable_device=$(find_compatible_device $emulator_api_level)
if [ "$usable_device" != "" ]; then
    echo "Suitable emulator already running!"
    exit 0
fi

echo "No suitable emulator found, checking AVD images..."
create_avd $emulator_api_level $android_arch
echo "Launching emulator!"
launch_emulator $emulator_api_level

echo "Setting up .NET $dotnet_ver..."
source ./prepare_dotnet.sh $dotnet_ver