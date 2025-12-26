using BepInEx;
using BepInEx.Logging;
using HarmonyLib;
using System.Collections.Generic;
using UnityEngine;
using System.Linq;

namespace LinuxWindowDancePlugin;

public class Patches
{

    [HarmonyPatch(typeof(PauseMenuContentData), "usesConsoleWindowMovement", MethodType.Getter)]
    [HarmonyPrefix]
    public static bool usesConsoleWindowMovement(ref bool __result)
    {
        __result = false;
        return false;
    }

    [HarmonyPatch(typeof(PauseMenuData), "Initialize")]
    [HarmonyPrefix]
    public static bool PauseMenuDataInitialize(
        PauseMenuData __instance,
        ref bool ___isInitialized,
        ref Dictionary<PauseContentName, PauseMenuContentData> ___pauseContentsDict,
        ref Dictionary<PauseModeName, PauseMenuModeData> ___pauseModesDict
    )
    {
        if (___isInitialized)
        {
            return false;
        }
        ___pauseContentsDict = new Dictionary<PauseContentName, PauseMenuContentData>();
        PauseMenuContentData[] array = __instance.contents;
        foreach (PauseMenuContentData pauseMenuContentData in array)
        {
            ___pauseContentsDict.Add(pauseMenuContentData.name, pauseMenuContentData);
        }
        ___pauseModesDict = new Dictionary<PauseModeName, PauseMenuModeData>();
        PauseMenuModeData[] array2 = __instance.modes;
        foreach (PauseMenuModeData pauseMenuModeData in array2)
        {
            ___pauseModesDict.Add(pauseMenuModeData.name, pauseMenuModeData);
        }
        ___isInitialized = true;
        return false;
    }

    [HarmonyPatch(typeof(RDStartup), "LoadPlatformHelpers")]
    [HarmonyPrefix]
    public static bool PauseMenuDataInitialize()
    {
        PlatformHelper.instance = new PlatformHelperLinux();
        return false;
    }

    [HarmonyPatch(typeof(RealWindowChoreographer), "Setup")]
    [HarmonyPrefix]
    public static bool RealWindowChoreographerSetup(
        int dancerCount,
        RealWindowChoreographer __instance,
        ref System.Collections.IEnumerator __result
    )
    {
        __result = RealWindowChoreographerSetupCoroutine(dancerCount, __instance);
        return false;
    }

    public static System.Collections.IEnumerator RealWindowChoreographerSetupCoroutine(
        int dancerCount,
        RealWindowChoreographer instance
    )
    {
        Plugin.Logger.LogDebug($"Adding {dancerCount} window dancer(s)");
        instance.dancers = new WindowDancer[dancerCount];
        for (int i = 0; i < dancerCount; i++)
        {
            Plugin.Logger.LogDebug($"Adding window {i}");
            Window window = (i == 0 && instance.mainWindowIsDancer) ? new UnityPlayerWindowLinux(i, instance) : new CustomWindowLinux(i, instance, transparent: true);
            Plugin.Logger.LogDebug($"Created class");
            if (window == null)
            {
                Plugin.Logger.LogError($"Window {i} was not initialized, it's null");
            }
            Plugin.Logger.LogDebug($"Creating window dancer");
            instance.dancers[i] = new WindowDancer(window, instance);
            Plugin.Logger.LogDebug($"Window was created");
        }
        Plugin.Logger.LogDebug("Minimizing if fullscreen...");
        yield return instance.MinimizeIfFullscreen();
        Plugin.Logger.LogDebug("Setting up rest...");
        if (!instance.mainWindowIsDancer)
        {
            if (!instance.editorMode)
            {
                WindowChoreographer.playerWindow.Reset(instance);
            }
        }
        WindowDancer[] array = instance.dancers;
        for (int j = 0; j < array.Length; j++)
        {
            array[j].window.ResetSize(instance);
        }
        if (instance.dancers.Any((WindowDancer dancer) => dancer.window is CustomWindow))
        {
            // instance.game.StartCoroutine((System.Collections.IEnumerator)AccessTools.Method(typeof(RealWindowChoreographer), "CustomWindowLoop").Invoke(instance, []));
            instance.game.StartCoroutine(instance.CustomWindowLoop());
        }
        instance.setup = true;
        Plugin.Logger.LogInfo("Window choreographer was set up");
    }

    [HarmonyPatch(typeof(scrVfxControl), "CreateRenderTexture", [typeof(Vector2Int), typeof(string), typeof(int)])]
    [HarmonyPrefix]
    public static bool CreateRenderTexture(ref RenderTexture __result, Vector2Int size, string name = "Debug", int depth = 16)
    {
        Vector2Int _size = size;
        __result = new RenderTexture(_size.x, _size.y, depth, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        __result.filterMode = FilterMode.Point;
        __result.name = name;
        return false;
    }

    [HarmonyPatch(typeof(scrVfxControl), "CheckForWindowDanceChanges")]
    [HarmonyPrefix]
    public static bool CheckForWindowDanceChanges()
    {
        WindowChoreographer.playerWindow.Show(true);
        return true;
    }
}
