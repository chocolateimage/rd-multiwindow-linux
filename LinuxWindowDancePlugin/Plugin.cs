using System;
using BepInEx;
using BepInEx.Logging;
using HarmonyLib;

namespace LinuxWindowDancePlugin;

[BepInPlugin("me.chocolateimage.linuxwindowdance", MyPluginInfo.PLUGIN_NAME, MyPluginInfo.PLUGIN_VERSION)]
public class Plugin : BaseUnityPlugin
{
    internal static new ManualLogSource Logger;
    internal static bool DebugLoggingEnabled;

    private static bool ReadDebugFlag()
    {
        string value = Environment.GetEnvironmentVariable("RD_DANCE_DEBUG") ?? string.Empty;
        value = value.Trim().ToLowerInvariant();
        return value.Length > 0 && value != "0" && value != "false" && value != "off" && value != "no";
    }

    internal static void LogDebug(string message)
    {
        if (DebugLoggingEnabled)
        {
            Logger.LogInfo($"[Debug] {message}");
        }
    }

    private void Awake()
    {
        // Plugin startup logic
        Logger = base.Logger;
        DebugLoggingEnabled = ReadDebugFlag();

        MultiWindow.Interop.NativeMethods.GetMainWindow(); // Just to load the plugin

        VulkanTextureBridge.EnsureCreated();

        Harmony.CreateAndPatchAll(typeof(Patches));

        Logger.LogInfo($"RD_DANCE_DEBUG={DebugLoggingEnabled}");
        Logger.LogInfo($"Graphics device type: {UnityEngine.SystemInfo.graphicsDeviceType}");
        if (VulkanTextureBridge.IsSupported)
        {
            Logger.LogInfo("Vulkan detected; custom windows will use native texture registration + plugin events.");
        }

        Logger.LogInfo($"Plugin {MyPluginInfo.PLUGIN_GUID} is loaded!");
    }
}
