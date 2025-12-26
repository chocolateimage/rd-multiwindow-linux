#!/bin/bash

set -eu

if [ $# -eq 0 ]; then
    echo "Usage: $0 \"/path/to/steamapps/common/Rhythm Doctor\" [--wayland]"
    exit 1
fi

if [ ! -f "$1/Rhythm Doctor" ]; then
    echo "ERROR: First argument must be a path to the Linux version of Rhythm Doctor."
    exit 1
fi

COMPILE_ARGUMENTS=""

if [ $# -eq 2 ]; then
    if [[ "$2" == "--wayland" ]]; then
        COMPILE_ARGUMENTS=" -DKWIN_WAYLAND "
    fi
fi

g++ -o multiwindow_unity.so -fPIC -shared multiwindow_unity.cpp `pkg-config Qt6Widgets Qt6DBus xcb --libs --cflags` -Og $COMPILE_ARGUMENTS
mv multiwindow_unity.so "$1/Rhythm Doctor_Data/Plugins/multiwindow_unity.so"
