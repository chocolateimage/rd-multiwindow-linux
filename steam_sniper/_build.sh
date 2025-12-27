#!/bin/bash

g++ -o multiwindow_unity.so -fPIC -shared multiwindow_unity.cpp `pkg-config Qt5Widgets Qt5DBus xcb glew --libs --cflags` -Og
