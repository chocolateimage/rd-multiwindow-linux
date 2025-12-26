using UnityEngine;

namespace LinuxWindowDancePlugin;

public static class Extensions {
    public static bool ContainsPoint(this PlatformHelper.Rectangle rectangle, Vector2Int point)
    {
        return point.y >= rectangle.Top && point.y <= rectangle.Bottom && point.x >= rectangle.Left && point.x <= rectangle.Right;
    }
}
