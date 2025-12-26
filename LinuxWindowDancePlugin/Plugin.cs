using System;
using BepInEx;
using BepInEx.Logging;
using HarmonyLib;

namespace LinuxWindowDancePlugin;

[BepInPlugin("me.chocolateimage.linuxwindowdance", MyPluginInfo.PLUGIN_NAME, MyPluginInfo.PLUGIN_VERSION)]
public class Plugin : BaseUnityPlugin
{
    internal static new ManualLogSource Logger;

    private void Awake()
    {
        // Plugin startup logic
        Logger = base.Logger;

        MultiWindow.Interop.NativeMethods.GetMainWindow(); // Just to load the plugin

        Harmony.CreateAndPatchAll(typeof(Patches));

        Logger.LogInfo($"Plugin {MyPluginInfo.PLUGIN_GUID} is loaded!");
    }
}
