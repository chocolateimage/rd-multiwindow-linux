
using System;
using System.Runtime.InteropServices;

namespace LinuxWindowDancePlugin;

public class Native
{
    public const int RenderEventVulkanCopy = 1;

    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture")]
    public static extern IntPtr SetWindowTexture(IntPtr window, IntPtr texturePtr);

    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture_size")]
    public static extern void SetWindowTextureSize(IntPtr window, int w, int h);

    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture_pixels")]
    public static extern void SetWindowTexturePixels(IntPtr window, byte[] textureBytes, int byteCount, int w, int h);

    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture_pixels")]
    public static extern void SetWindowTexturePixelsPtr(IntPtr window, IntPtr textureBytes, int byteCount, int w, int h);

    [DllImport("multiwindow_unity", EntryPoint = "get_monitors")]
    public static extern NativeMonitors GetMonitors();

    [DllImport("multiwindow_unity", EntryPoint = "get_info")]
    public static extern string GetInfo();

    [DllImport("multiwindow_unity", EntryPoint = "get_render_event_func")]
    public static extern IntPtr GetRenderEventFunc();
}
