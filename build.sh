#!/bin/bash

set -eu

if [ $# -eq 0 ]; then
    echo "Usage: $0 \"/path/to/steamapps/common/Rhythm Doctor\" [--wayland]"
    exit 1
fi

if [ ! -f "$1/Rhythm Doctor.exe" ]; then
    echo "ERROR: First argument must be a path to the Proton version of Rhythm Doctor."
    exit 1
fi

COMPILE_ARGUMENTS=""

if [ $# -eq 2 ]; then
    if [[ "$2" == "--wayland" ]]; then
        COMPILE_ARGUMENTS=" -DKWIN_WAYLAND "
    fi
fi

wineg++ -o multiwindow_unity.dll -shared multiwindow_unity.cpp multiwindow_unity.dll.spec `pkg-config Qt6Widgets Qt6DBus xcb --libs --cflags` -ld3d11 -O3 $COMPILE_ARGUMENTS
mv multiwindow_unity.dll.so "$1/Rhythm Doctor_Data/Plugins/x86_64/multiwindow_unity.dll"
