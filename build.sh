#!/bin/bash

set -eu

wineg++ -o multiwindow_unity.dll -shared multiwindow_unity.cpp multiwindow_unity.dll.spec `pkg-config Qt6Widgets xcb --libs --cflags`
mv multiwindow_unity.dll.so "$1/Rhythm Doctor_Data/Plugins/x86_64/multiwindow_unity.dll"

