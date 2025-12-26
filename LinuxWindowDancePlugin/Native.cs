
using System;
using System.Runtime.InteropServices;

namespace LinuxWindowDancePlugin;

public class Native
{
    [DllImport("multiwindow_unity", EntryPoint = "set_window_texture_size")]
    public static extern void SetWindowTextureSize(IntPtr window, int w, int h);
}
