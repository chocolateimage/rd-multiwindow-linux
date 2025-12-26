using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace LinuxWindowDancePlugin;

[StructLayout(LayoutKind.Sequential)]
public struct NativeMonitor
{
    public PlatformHelper.Monitor ToMonitor()
    {
        return new PlatformHelper.Monitor(
            new PlatformHelper.Rectangle()
            {
                Left = X,
                Top = Y,
                Right = X + Width,
                Bottom = Y + Height
            },
            Scale
        );
    }

    public int X;
    public int Y;
    public int Width;
    public int Height;
    public int Scale;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeMonitors
{
    public List<NativeMonitor> ToList()
    {
        List<NativeMonitor> nativeMonitors = new List<NativeMonitor>();
        for (int i = 0; i < monitorCount; i++)
        {
            nativeMonitors.Add(monitors[i]);
        }
        return nativeMonitors;
    }

    public NativeMonitor* monitors;
    public int monitorCount;
}
