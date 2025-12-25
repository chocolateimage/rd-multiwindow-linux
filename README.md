# Rhythm Doctor Multi Window Plugin for Linux

[![Screenshot of a Rhythm Doctor window moving](https://party.playlook.de/public/Screenshot_20251222_165039.png)](https://youtu.be/_Lh0sd66jjI)

Demonstration Videos:

- [2-X All The Times](https://youtu.be/_Lh0sd66jjI)
- [7-X Miracle Defibrillator](https://youtu.be/_Lh0sd66jjI) (outdated video)

Other tools: [![Logo](https://party.playlook.de/public/qrhythmcafe-icon.png) QRhythmCafe - Download Rhythm Doctor levels](https://github.com/chocolateimage/qrhythmcafe)

## Instructions

> [!NOTE]
> Only KDE Plasma (KWin) is supported. You can try to use it on other desktop environments/window managers, but it may break.

To build, you need these packages:

```bash
# Debian/Ubuntu based
sudo apt install libwine-dev pkg-config qt6-base-dev libxcb1-dev

# Arch
sudo pacman -S wine pkgconf qt6-base libxcb
```

> [!IMPORTANT]
> You need Proton enabled for the game. Go to Steam, right click the game, select "Properties", go to the "Compatibility" tab, and check "Force the use of a specific Steam Play compatibility tool". Make sure to select a "Proton ..." version. I tested it with "Proton Experimental".
>
> Note that you can no longer play the game from Steam itself after installing, you need to run the command below to run the game. To revert the change, go to the properties window, select the "Installed Files" tab, then click on "Verify Integrity of game files".

Usage:

```bash
./build.sh "/path/to/steamapps/common/Rhythm Doctor"

# OR this if you want experimental Wayland support (only KWin is supported, else it falls back to X11). This allows real offscreen windows.

./build.sh "/path/to/steamapps/common/Rhythm Doctor" --wayland
```

Steam's Proton doesn't work (at least not for me), so you have to manually run Wine, and in some cases in a different Wine Prefix to fix graphical issues:

```bash
cd "/path/to/steamapps/common/Rhythm Doctor"
WINEPREFIX="$HOME/.winerd" wine "Rhythm Doctor.exe"
```

If you found this plugin useful or cool, consider starring the GitHub repo!

## Issue Reporting

Before reporting an issue, follow these troubleshooting steps:

- Pull and rebuild the latest changes (`git pull` and `./build.sh ...`)
- Try the latest Wine **Staging** release. If using a non-rolling distro, use one from [WineHQ](https://gitlab.winehq.org/wine/wine/-/wikis/Download).

Put the distro you are using and desktop environment in your issue report.

Make sure to launch the game in the terminal. Provide the logs (not Player.log, but you can provide that too if you want) in a new GitHub issue.

If using the Wayland version, make sure to open `journalctl -ef` before launching the game, then provide the logs from there too.
