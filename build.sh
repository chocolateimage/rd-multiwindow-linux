#!/bin/bash

set -eu

if [ $# -ne 1 ]; then
    echo "Usage: $0 \"/path/to/steamapps/common/Rhythm Doctor\""
    exit 1
fi

if [ ! -f "$1/Rhythm Doctor.exe" ]; then
    echo "ERROR: First argument must be a path to the Proton version of Rhythm Doctor."
    exit 1
fi

wineg++ -o multiwindow_unity.dll -shared multiwindow_unity.cpp multiwindow_unity.dll.spec `pkg-config Qt6Widgets Qt6DBus xcb --libs --cflags` -ld3d11 -O3
mv multiwindow_unity.dll.so "$1/Rhythm Doctor_Data/Plugins/x86_64/multiwindow_unity.dll"
