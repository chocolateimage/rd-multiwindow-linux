#!/bin/bash

# Script inside the Docker runtime

g++ -o "steam_sniper/package/Rhythm Doctor_Data/Plugins/multiwindow_unity.so" -fPIC -shared multiwindow_unity.cpp `pkg-config Qt5Widgets Qt5DBus xcb glew --libs --cflags` -Og -DKWIN_WAYLAND -DSTEAM_RUNTIME
cp /usr/lib/x86_64-linux-gnu/libQt5WaylandClient.so.5 "steam_sniper/package/"
cp -r /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms "steam_sniper/package/"
cp -r /usr/lib/x86_64-linux-gnu/qt5/plugins/wayland-decoration-client "steam_sniper/package/"
cp -r /usr/lib/x86_64-linux-gnu/qt5/plugins/wayland-graphics-integration-client "steam_sniper/package/"
cp -r /usr/lib/x86_64-linux-gnu/qt5/plugins/wayland-graphics-integration-server "steam_sniper/package/"
cp -r /usr/lib/x86_64-linux-gnu/qt5/plugins/wayland-shell-integration "steam_sniper/package/"
