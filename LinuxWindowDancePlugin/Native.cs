
using System;
using System.Runtime.InteropServices;

namespace LinuxWindowDancePlugin;

public class Native
{
    [DllImport("multiwindow_unity", EntryPoint = "get_main_window")]
    public static extern IntPtr GetMainWindow();

    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture")]
    public static extern string SetWindowTexture(IntPtr window, IntPtr texturePtr);
}
