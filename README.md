# Rhythm Doctor Multi Window Plugin for Linux

[![Screenshot of a Rhythm Doctor window moving](https://party.playlook.de/public/Screenshot_20251222_165039.png)](https://youtu.be/_Lh0sd66jjI)

Demonstration Videos:

- [2-X All The Times](https://youtu.be/_Lh0sd66jjI)
- [7-X Miracle Defibrillator](https://youtu.be/JAlYj7la_B4)

Other tools: [![Logo](https://party.playlook.de/public/qrhythmcafe-icon.png) QRhythmCafe - Download Rhythm Doctor levels](https://github.com/chocolateimage/qrhythmcafe)

## Instructions

> [!IMPORTANT]
> ⚠️ Only KDE Plasma 6.5+ (KWin) and Hyprland are supported. You can try to use it on other desktop environments/window managers, but it may break.

You can either choose from these versions:

- [Native Linux](#native-linux)
- [Proton](#proton)

### Native Linux

The native Linux version works with BepInEx 5. BepInEx is a mod loader for games. If you do not want to use BepInEx, use the [Proton version](#proton).

1. You'll need BepInEx 5 installed. If you do not have it, follow the [install guide](https://docs.bepinex.dev/articles/user_guide/installation/index.html?tabs=tabid-nix), then also follow the [Steam guide](https://docs.bepinex.dev/articles/advanced/steam_interop.html).

1. [Download the latest release ZIP](https://github.com/chocolateimage/rd-multiwindow-linux/releases/latest/download/linux-steam-runtime-multiwindow.zip) and extract it to the root of the game. `libQt6#######.so.6` files should be in the same folder as `UnityPlayer.so` and `run_bepinex.sh`.

1. In the Steam compatibility settings, do not force the runtime.

1. Set Steam launch options:
    - **KDE Plasma (and rest):** `./run_bepinex.sh %command%`
    - **Hyprland:** `PRESSURE_VESSEL_FILESYSTEMS_RW="$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE" ./run_bepinex.sh %command%`

1. Open the game, go to the settings and select the window dance option in the accessibility tab. Have fun!

If you found this plugin useful or cool, consider starring the GitHub repo!

---

### Proton

This is the version when you have "Proton Experimental" selected in the compatibility list. The game would not run on Proton, but only through regular Wine which will be explained later.

To build, you need these packages:

```bash
# Debian/Ubuntu based (if on Wine Staging, replace "libwine-dev" with wine-staging-dev)
sudo apt install libwine-dev pkg-config qt6-base-dev libxcb1-dev git

# Arch
sudo pacman -S --needed wine-staging pkgconf qt6-base libxcb git
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
```

Steam's Proton doesn't work (at least not for me), so you have to manually run Wine, and in some cases in a different Wine Prefix to fix graphical issues:

```bash
cd "/path/to/steamapps/common/Rhythm Doctor"
WINEPREFIX="$HOME/.winerd" wine "Rhythm Doctor.exe"
```

If you found this plugin useful or cool, consider starring the GitHub repo!

## Issue Reporting

Before reporting an issue, follow these troubleshooting steps **for the native version**:

- Try the latest release of the plugin. Override any files if it asks to.
- If there is an option for window dance in the settings, select the opton and read what it says on the right.
- In `Rhythm Doctor/BepInEx/config/BepInEx.cfg`, enable `[Logging.Console]`'s `Enabled = true` and enable `[Chainloader]`'s `HideManagerGameObject = true`.

And these troubleshooting steps **if only you are using the Wine version**:

- Pull and rebuild the latest changes (`git pull` and `./build.sh ...`)
- Try the latest Wine **Staging** release. If using a non-rolling distro, use one from [WineHQ](https://gitlab.winehq.org/wine/wine/-/wikis/Download).

What to include in your report:

- Put the distro you are using and desktop environment in your issue report.
- Make sure to launch the game in the terminal. Provide the logs (not Player.log, but you can provide that too if you want) in a new GitHub issue.
- If using the Wayland version, make sure to open `journalctl -ef` before launching the game, then provide the logs from there too.
- A screen recording can help me see the problem.

## Development

> [!NOTE]
> This section is only if you want to **edit** the code, not if you want to run it.

You need some things installed:

- .NET SDK for BepInEx. Arch: `dotnet-sdk` Fedora: `dotnet-sdk-10.0`
- Build packages: Arch: `pkgconf qt6-base libxcb` Fedora: `qt6-qtbase-devel glew-devel`
- The VS Code extensions clangd and C# Dev Kit
- _Docker (optional, only if you want to build Steam-sandbox-compatible files)_

To build the BepInEx plugin itself (the .dll):

```bash
cd LinuxWindowDancePlugin
# You will need to have Rhythm Doctor installed in Steam for this to work
dotnet build
```

The DLL will be at `LinuxWindowDancePlugin/bin/Debug/netstandard2.1/LinuxWindowDancePlugin.dll`. You can symlink your plugin in the game folder to that newly built DLL.

Then if you want to build the Linux `.so` file:

```bash
./build.sh "/path/to/steamapps/common/Rhythm Doctor"
```

### Steam Runtime Sniper Build

If you want to build a version that works inside the Steam Sniper sandbox, you will need Docker (or any compatible software, like Podman).

```bash
# Download image and compile Qt6
./steam_sniper/prepare.sh

# Create the Steam Runtime Sniper ZIP file
./steam_sniper/build.sh
```

You can then extract the ZIP file on top of your game directory.
