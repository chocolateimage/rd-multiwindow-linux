using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace LinuxWindowDancePlugin;

public sealed class VulkanTextureBridge : MonoBehaviour
{
    private sealed class TrackedWindow
    {
        public CustomWindowLinux Window = null!;
        public bool LoggedMissingSourceTexture;
        public bool LoggedMissingWindowPtr;
        public bool LoggedMissingNativeTexture;
        public bool LoggedRegisteredTexture;
        public int NativeTextureWidth = -1;
        public int NativeTextureHeight = -1;
        public IntPtr NativeTexturePtr = IntPtr.Zero;
    }

    private static VulkanTextureBridge? instance;
    private readonly List<TrackedWindow> trackedWindows = new();
    private readonly WaitForEndOfFrame endOfFrame = new();

    public static bool IsSupported => SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan;

    public static void EnsureCreated()
    {
        if (!IsSupported || instance != null)
        {
            return;
        }

        Plugin.LogDebug("Creating VulkanTextureBridge helper GameObject.");
        GameObject gameObject = new("LinuxWindowDancePlugin.VulkanTextureBridge");
        gameObject.hideFlags = HideFlags.HideAndDontSave;
        DontDestroyOnLoad(gameObject);
        instance = gameObject.AddComponent<VulkanTextureBridge>();
    }

    public static void Register(CustomWindowLinux window)
    {
        if (!IsSupported || window == null)
        {
            return;
        }

        EnsureCreated();
        instance?.RegisterInternal(window);
    }

    private void Awake()
    {
        if (instance != null && instance != this)
        {
            Destroy(gameObject);
            return;
        }

        instance = this;
        DontDestroyOnLoad(gameObject);
        Plugin.Logger.LogInfo("VulkanTextureBridge Awake -> using native Vulkan texture registration + plugin events.");
        StartCoroutine(RenderLoop());
    }

    private void RegisterInternal(CustomWindowLinux window)
    {
        foreach (TrackedWindow trackedWindow in trackedWindows)
        {
            if (ReferenceEquals(trackedWindow.Window, window))
            {
                Plugin.LogDebug($"Skip duplicate Vulkan window registration: ptr=0x{window.Window.WindowPtr.ToInt64():X}");
                return;
            }
        }

        Plugin.Logger.LogInfo(
            $"Register Vulkan window: ptr=0x{window.Window.WindowPtr.ToInt64():X}, rt={(window.renderTexture != null ? window.renderTexture.width + "x" + window.renderTexture.height : "null")}");
        trackedWindows.Add(new TrackedWindow { Window = window });
    }

    private IEnumerator RenderLoop()
    {
        while (true)
        {
            yield return endOfFrame;

            bool issuedAnyVulkanWork = false;

            for (int i = trackedWindows.Count - 1; i >= 0; i--)
            {
                try
                {
                    if (!UpdateWindowRegistration(trackedWindows[i], ref issuedAnyVulkanWork))
                    {
                        trackedWindows.RemoveAt(i);
                    }
                }
                catch (Exception ex)
                {
                    Plugin.Logger.LogWarning($"Vulkan texture bridge update failed: {ex}");
                }
            }

            if (issuedAnyVulkanWork)
            {
                GL.IssuePluginEvent(Native.GetRenderEventFunc(), Native.RenderEventVulkanCopy);
            }
        }
    }

    private static void EnsureNativeTextureSize(TrackedWindow trackedWindow, IntPtr windowPtr, int width, int height)
    {
        if (trackedWindow.NativeTextureWidth == width && trackedWindow.NativeTextureHeight == height)
        {
            return;
        }

        Native.SetWindowTextureSize(windowPtr, width, height);
        trackedWindow.NativeTextureWidth = width;
        trackedWindow.NativeTextureHeight = height;
    }

    private static bool UpdateWindowRegistration(TrackedWindow trackedWindow, ref bool issuedAnyVulkanWork)
    {
        if (trackedWindow.Window == null || trackedWindow.Window.Window == null)
        {
            Plugin.Logger.LogWarning("Vulkan bridge stopped because target window reference is gone.");
            return false;
        }

        RenderTexture sourceTexture = trackedWindow.Window.renderTexture;
        if (sourceTexture == null)
        {
            if (!trackedWindow.LoggedMissingSourceTexture)
            {
                Plugin.Logger.LogWarning($"Vulkan bridge waiting for RenderTexture: ptr=0x{trackedWindow.Window.Window.WindowPtr.ToInt64():X}");
                trackedWindow.LoggedMissingSourceTexture = true;
            }
            return true;
        }
        trackedWindow.LoggedMissingSourceTexture = false;

        IntPtr windowPtr = trackedWindow.Window.Window.WindowPtr;
        if (windowPtr == IntPtr.Zero)
        {
            if (!trackedWindow.LoggedMissingWindowPtr)
            {
                Plugin.Logger.LogWarning("Vulkan bridge waiting for native window pointer (still zero).");
                trackedWindow.LoggedMissingWindowPtr = true;
            }
            return true;
        }
        trackedWindow.LoggedMissingWindowPtr = false;

        if (sourceTexture.width <= 0 || sourceTexture.height <= 0 || !sourceTexture.IsCreated())
        {
            return true;
        }

        EnsureNativeTextureSize(trackedWindow, windowPtr, sourceTexture.width, sourceTexture.height);

        IntPtr nativeTexturePtr = sourceTexture.GetNativeTexturePtr();
        if (nativeTexturePtr == IntPtr.Zero)
        {
            if (!trackedWindow.LoggedMissingNativeTexture)
            {
                Plugin.Logger.LogWarning($"Vulkan bridge waiting for native texture pointer: ptr=0x{windowPtr.ToInt64():X}");
                trackedWindow.LoggedMissingNativeTexture = true;
            }
            return true;
        }
        trackedWindow.LoggedMissingNativeTexture = false;

        if (trackedWindow.NativeTexturePtr != nativeTexturePtr)
        {
            Native.SetWindowTexture(windowPtr, nativeTexturePtr);
            trackedWindow.NativeTexturePtr = nativeTexturePtr;

            Plugin.Logger.LogInfo(
                $"Registered Vulkan texture: window=0x{windowPtr.ToInt64():X}, texture=0x{nativeTexturePtr.ToInt64():X}, size={sourceTexture.width}x{sourceTexture.height}");
            trackedWindow.LoggedRegisteredTexture = true;
        }

        if (Plugin.DebugLoggingEnabled && trackedWindow.LoggedRegisteredTexture)
        {
            Plugin.LogDebug(
                $"Queued Vulkan plugin event: window=0x{windowPtr.ToInt64():X}, texture=0x{nativeTexturePtr.ToInt64():X}, size={sourceTexture.width}x{sourceTexture.height}, format={sourceTexture.graphicsFormat}");
        }

        issuedAnyVulkanWork = true;
        return true;
    }
}