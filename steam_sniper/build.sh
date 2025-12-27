#!/bin/bash

# Creates the Steam Runtime Sniper ZIP file. Run this script from the root of the project like this: `./steam_sniper/build.sh`
# Make sure you ran `./steam_sniper/prepare.sh` before.

rm -rv ./steam_sniper/package
mkdir -p ./steam_sniper/package
mkdir -p "./steam_sniper/package/Rhythm Doctor_Data/Plugins"
mkdir -p "./steam_sniper/package/BepInEx/plugins"

echo
echo "Building..."

docker run -w /app -v .:/app -u "$(id -u):$(id -g)" --rm --init -it rd-multiwindow-linux:latest /bin/bash steam_sniper/_build.sh
pushd LinuxWindowDancePlugin
dotnet build -c Release
popd

cp "LinuxWindowDancePlugin/bin/Release/netstandard2.1/LinuxWindowDancePlugin.dll" "./steam_sniper/package/BepInEx/plugins/"

pushd steam_sniper/package
zip -r ../../linux-steam-runtime-multiwindow.zip .
popd
