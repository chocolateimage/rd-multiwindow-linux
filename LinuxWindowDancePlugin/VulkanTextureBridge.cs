using System;
using System.Collections;
using System.Collections.Generic;
using Unity.Collections;
using UnityEngine;
using UnityEngine.Rendering;

namespace LinuxWindowDancePlugin;

public sealed class VulkanTextureBridge : MonoBehaviour
{
    private sealed class TrackedWindow
    {
        public CustomWindowLinux Window = null!;
        public Texture2D? StagingTexture;
        public int CaptureCount;
        public bool LoggedMissingSourceTexture;
        public bool LoggedMissingWindowPtr;
        public bool LoggedFirstSuccessfulUpload;
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
        Plugin.LogDebug("VulkanTextureBridge Awake -> starting capture loop.");
        StartCoroutine(CaptureLoop());
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

    private IEnumerator CaptureLoop()
    {
        while (true)
        {
            yield return endOfFrame;

            for (int i = trackedWindows.Count - 1; i >= 0; i--)
            {
                try
                {
                    if (!CaptureWindow(trackedWindows[i]))
                    {
                        trackedWindows.RemoveAt(i);
                    }
                }
                catch (Exception ex)
                {
                    Plugin.Logger.LogWarning($"Vulkan texture capture failed: {ex}");
                }
            }
        }
    }

    private static Texture2D EnsureStagingTexture(TrackedWindow trackedWindow, int width, int height)
    {
        if (trackedWindow.StagingTexture != null && trackedWindow.StagingTexture.width == width && trackedWindow.StagingTexture.height == height)
        {
            return trackedWindow.StagingTexture;
        }

        if (trackedWindow.StagingTexture != null)
        {
            Destroy(trackedWindow.StagingTexture);
        }

        trackedWindow.StagingTexture = new Texture2D(width, height, TextureFormat.RGBA32, false, false)
        {
            filterMode = FilterMode.Point,
            wrapMode = TextureWrapMode.Clamp,
            name = $"VulkanWindowBridge_{width}x{height}"
        };

        Plugin.Logger.LogInfo($"Create Vulkan staging texture: {width}x{height}");
        Native.SetWindowTextureSize(trackedWindow.Window.Window.WindowPtr, width, height);
        return trackedWindow.StagingTexture;
    }

    private static string DescribeSample(byte[] rawData, int width, int height)
    {
        if (rawData.Length < 4 || width <= 0 || height <= 0)
        {
            return "sample=unavailable";
        }

        static string PixelAt(byte[] data, int widthValue, int heightValue, int x, int y)
        {
            x = Mathf.Clamp(x, 0, widthValue - 1);
            y = Mathf.Clamp(y, 0, heightValue - 1);
            int index = (y * widthValue + x) * 4;
            if (index + 3 >= data.Length)
            {
                return "(out-of-range)";
            }

            return $"({data[index + 0]},{data[index + 1]},{data[index + 2]},{data[index + 3]})";
        }

        return $"tl={PixelAt(rawData, width, height, 0, 0)} center={PixelAt(rawData, width, height, width / 2, height / 2)} br={PixelAt(rawData, width, height, width - 1, height - 1)}";
    }

    private static bool CaptureWindow(TrackedWindow trackedWindow)
    {
        if (trackedWindow.Window == null || trackedWindow.Window.Window == null)
        {
            Plugin.Logger.LogWarning("Vulkan capture stopped because target window reference is gone.");
            return false;
        }

        RenderTexture sourceTexture = trackedWindow.Window.renderTexture;
        if (sourceTexture == null)
        {
            if (!trackedWindow.LoggedMissingSourceTexture)
            {
                Plugin.Logger.LogWarning($"Vulkan capture waiting for RenderTexture: ptr=0x{trackedWindow.Window.Window.WindowPtr.ToInt64():X}");
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
                Plugin.Logger.LogWarning("Vulkan capture waiting for native window pointer (still zero).");
                trackedWindow.LoggedMissingWindowPtr = true;
            }
            return true;
        }
        trackedWindow.LoggedMissingWindowPtr = false;

        Texture2D stagingTexture = EnsureStagingTexture(trackedWindow, sourceTexture.width, sourceTexture.height);
        RenderTexture previousRenderTexture = RenderTexture.active;

        if (Plugin.DebugLoggingEnabled && (trackedWindow.CaptureCount < 3 || trackedWindow.CaptureCount % 120 == 0))
        {
            Plugin.LogDebug(
                $"CaptureWindow begin: ptr=0x{windowPtr.ToInt64():X}, rt={sourceTexture.width}x{sourceTexture.height}, created={sourceTexture.IsCreated()}, format={sourceTexture.format}, graphicsFormat={sourceTexture.graphicsFormat}");
        }

        try
        {
            RenderTexture.active = sourceTexture;
            stagingTexture.ReadPixels(new Rect(0, 0, sourceTexture.width, sourceTexture.height), 0, 0, false);
            stagingTexture.Apply(false, false);
        }
        finally
        {
            RenderTexture.active = previousRenderTexture;
        }

        NativeArray<byte> rawDataView = stagingTexture.GetRawTextureData<byte>();
        byte[] rawData = rawDataView.ToArray();
        Native.SetWindowTexturePixels(windowPtr, rawData, rawData.Length, sourceTexture.width, sourceTexture.height);

        if (!trackedWindow.LoggedFirstSuccessfulUpload ||
            (Plugin.DebugLoggingEnabled && (trackedWindow.CaptureCount < 3 || trackedWindow.CaptureCount % 120 == 0)))
        {
            Plugin.Logger.LogInfo(
                $"Vulkan upload frame {trackedWindow.CaptureCount} -> ptr=0x{windowPtr.ToInt64():X}, bytes={rawData.Length}, {DescribeSample(rawData, sourceTexture.width, sourceTexture.height)}");
            trackedWindow.LoggedFirstSuccessfulUpload = true;
        }

        trackedWindow.CaptureCount++;
        return true;
    }
}