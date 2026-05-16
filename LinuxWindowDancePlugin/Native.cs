
using System;
using System.Runtime.InteropServices;

namespace LinuxWindowDancePlugin;

public class Native
{
    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture_size")]
    public static extern void SetWindowTextureSize(IntPtr window, int w, int h);

    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture_pixels")]
    public static extern void SetWindowTexturePixels(IntPtr window, byte[] textureBytes, int byteCount, int w, int h);

    [DllImport("multiwindow_unity", EntryPoint = "get_monitors")]
    public static extern NativeMonitors GetMonitors();

    [DllImport("multiwindow_unity", EntryPoint = "get_info")]
    public static extern string GetInfo();
}
