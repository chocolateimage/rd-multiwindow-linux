# Rhythm Doctor Multi Window Plugin for Linux

To build, you need these packages:

```bash
# Debian/Ubuntu based
sudo apt install libwine-dev pkg-config qt6-base-dev libxcb1-dev

# Arch
sudo pacman -S wine pkgconf qt6-base libxcb
```

Usage:

```bash
./build.sh "/path/to/steamapps/common/Rhythm Doctor"
```

Steam's Proton doesn't work (at least not for me), so you have to manually run Wine, and in some cases in a different Wine Prefix to fix graphical issues:

```bash
cd "/path/to/steamapps/common/Rhythm Doctor"
WINEPREFIX="$HOME/.winerd" wine "Rhythm Doctor.exe"
```
