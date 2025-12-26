using UnityEngine;

namespace LinuxWindowDancePlugin;

public class CustomWindowLinux : CustomWindow
{
    public CustomWindowLinux(int index, WindowChoreographer choreographer, bool transparent) : base(index, choreographer, transparent)
    {
        Native.SetWindowTextureSize(Window.WindowPtr, renderTexture.width, renderTexture.height);
    }
}
