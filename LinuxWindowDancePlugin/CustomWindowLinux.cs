using UnityEngine;

namespace LinuxWindowDancePlugin;

public class CustomWindowLinux : CustomWindow
{
    public CustomWindowLinux(int index, WindowChoreographer choreographer, bool transparent) : base(index, choreographer, transparent)
    {
        Native.SetWindowTextureSize(Window.WindowPtr, renderTexture.width, renderTexture.height);
    }

    public override Vector2Int GetPosition()
    {
        var pos = Window.GetPosition();
        var size = Window.GetSize();
        PlatformHelperLinux platformHelperLinux = platformHelper as PlatformHelperLinux;
        int y = platformHelperLinux.WindowDanceResolution.y;
        Vector2Int point = platformHelperLinux.GetBottomLeftDesktopPoint();
        return new Vector2Int(pos.x - point.x, y - (pos.y + size.height - point.y));
    }
}
