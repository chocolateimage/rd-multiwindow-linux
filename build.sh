#!/bin/bash

set -eu

if [ $# -eq 0 ]; then
    echo "Usage: $0 \"/path/to/steamapps/common/Rhythm Doctor\" [--wayland]"
    exit 1
fi

COMPILE_ARGUMENTS=""
IS_WINE=0

if [ -f "$1/Rhythm Doctor.exe" ]; then

    echo "Windows version detected"
    COMPILE_ARGUMENTS="$COMPILE_ARGUMENTS -DWITH_WINE "
    IS_WINE=1

elif [ -f "$1/Rhythm Doctor" ]; then
    echo "Linux version detected"

    if [ ! -f "$1/run_bepinex.sh" ]; then
        echo "ERROR: BepInEx not installed."
        exit 1
    fi

    if [ -f "LinuxWindowDancePlugin/bin/Debug/netstandard2.1/LinuxWindowDancePlugin.dll" ]; then
        echo "Local plugin detected, not downloading."
    else
        echo "Downloading plugin..."
        mkdir -p "$1/BepInEx/plugins"
        curl -Lso "$1/BepInEx/plugins/LinuxWindowDancePlugin.dll" "https://github.com/chocolateimage/rd-multiwindow-linux/releases/download/bepinex/LinuxWindowDancePlugin.dll"
        echo "Finished downloading. Building..."
    fi
else
    echo "ERROR: First argument must be a path to Rhythm Doctor."
    exit 1
fi

if [ $# -eq 2 ]; then
    if [[ "$2" == "--wayland" ]]; then
        COMPILE_ARGUMENTS="$COMPILE_ARGUMENTS -DKWIN_WAYLAND "
        echo "Wayland enabled"
    fi
fi

if [ $IS_WINE -eq 1 ]; then
    wineg++ -o multiwindow_unity.dll -shared multiwindow_unity.cpp multiwindow_unity.dll.spec `pkg-config Qt6Widgets Qt6DBus xcb --libs --cflags` -ld3d11 -O3 $COMPILE_ARGUMENTS
    mv multiwindow_unity.dll.so "$1/Rhythm Doctor_Data/Plugins/x86_64/multiwindow_unity.dll"
else
    g++ -o multiwindow_unity.so -fPIC -shared multiwindow_unity.cpp `pkg-config Qt6Widgets Qt6DBus xcb glew --libs --cflags` -Og $COMPILE_ARGUMENTS
    mv multiwindow_unity.so "$1/Rhythm Doctor_Data/Plugins/multiwindow_unity.so"
fi

echo "Finished"
