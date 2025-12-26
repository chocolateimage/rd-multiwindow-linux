# Rhythm Doctor Multi Window Plugin for Linux

[![Screenshot of a Rhythm Doctor window moving](https://party.playlook.de/public/Screenshot_20251222_165039.png)](https://youtu.be/_Lh0sd66jjI)

Demonstration Videos:

- [2-X All The Times](https://youtu.be/_Lh0sd66jjI)
- [7-X Miracle Defibrillator](https://youtu.be/_Lh0sd66jjI) (outdated video)

Other tools: [![Logo](https://party.playlook.de/public/qrhythmcafe-icon.png) QRhythmCafe - Download Rhythm Doctor levels](https://github.com/chocolateimage/qrhythmcafe)

## Instructions

> [!NOTE]
> Only KDE Plasma (KWin) is supported. You can try to use it on other desktop environments/window managers, but it may break.

You can either choose from these versions:

- [Native Linux](#native-linux)
- [Proton](#proton)

### Native Linux

The native Linux version works with BepInEx 5. BepInEx is a mod loader for games. If you do not want to use BepInEx, use the [Proton version](#proton).

1. You'll need BepInEx 5 installed. If you do not have it, follow the [install guide](https://docs.bepinex.dev/articles/user_guide/installation/index.html?tabs=tabid-nix), then also follow the [Steam guide](https://docs.bepinex.dev/articles/advanced/steam_interop.html).

2. Install these packages:

    ```bash
    # Debian/Ubuntu based
    sudo apt install pkg-config qt6-base-dev libxcb1-dev git curl

    # Arch
    sudo pacman -S --needed pkgconf qt6-base libxcb git curl
    ```

3. Clone this project:

    ```bash
    git clone https://github.com/chocolateimage/rd-multiwindow-linux.git
    cd rd-multiwindow-linux
    ```

4. Build (will also download the BepInEx plugin):

    ```bash
    ./build.sh "/path/to/steamapps/common/Rhythm Doctor"

    # OR this if you want Wayland support (only KWin is supported, else it falls back to X11). This allows real offscreen windows.

    ./build.sh "/path/to/steamapps/common/Rhythm Doctor" --wayland
    ```

5. In the Steam compatibility settings, force the runtime to "Legacy runtime 1.0"
    ![Legacy runtime forced](https://party.playlook.de/public/steam-rd-legacy.png)

6. Open the game, go to the settings and select the window dance option in the accessibility tab. Have fun!

If you found this plugin useful or cool, consider starring the GitHub repo!

---

### Proton

This is the version when you have "Proton Experimental" selected in the compatibility list. The game would not run on Proton, but only through regular Wine which will be explained later.

To build, you need these packages:

```bash
# Debian/Ubuntu based
sudo apt install libwine-dev pkg-config qt6-base-dev libxcb1-dev libglew-dev git

# Arch
sudo pacman -S --needed wine-staging pkgconf qt6-base libxcb glew git
```

Clone this project:

```bash
git clone https://github.com/chocolateimage/rd-multiwindow-linux.git
cd rd-multiwindow-linux
```

> [!NOTE]
> Note that you can no longer play the game from Steam itself after installing, you need to run the command below to run the game. To revert the change, go to the properties window, select the "Installed Files" tab, then click on "Verify Integrity of game files".

To patch the game:

```bash
./build.sh "/path/to/steamapps/common/Rhythm Doctor"

# OR this if you want Wayland support (only KWin is supported, else it falls back to X11). This allows real offscreen windows.

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
