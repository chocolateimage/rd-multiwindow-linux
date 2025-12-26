namespace LinuxWindowDancePlugin;

public class UnityPlayerWindowLinux : UnityPlayerWindow
{
    public UnityPlayerWindowLinux(int index, WindowChoreographer choreographer) : base(index, choreographer)
    {
    }

    public override bool IsInWindowDanceRect()
    {
        throw new System.NotImplementedException();
    }
}
