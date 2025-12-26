using System.Collections.Generic;
using UnityEngine;

namespace LinuxWindowDancePlugin;

public class PlatformHelperLinux : PlatformHelper
{
    public Rectangle windowDanceResolutionRect;

    public PlatformHelperLinux()
    {
        playerWindow = new UnityPlayerWindowLinux(-1, null);
    }

    public override Vector2Int GetWindowDanceResolution()
    {
        windowDanceResolutionRect = GetMonitorsRect(Monitors, onlyHorizontal: false);
        return new Vector2Int(
            windowDanceResolutionRect.Right - windowDanceResolutionRect.Left,
            windowDanceResolutionRect.Bottom - windowDanceResolutionRect.Top
        );
    }

    public override List<Monitor> GetMonitors()
    {
        // TODO: Replace with real values
        return [
            new Monitor(new Rectangle() {
                Left = 0,
                Top = 0,
                Right = 1920,
                Bottom = 1080,
            }, 1)
        ];
    }

    public Vector2Int GetBottomLeftDesktopPoint()
    {
        Vector2Int result = new Vector2Int(windowDanceResolutionRect.Left, windowDanceResolutionRect.Top);
        foreach (Monitor monitor in Monitors)
        {
            if (monitor.bounds.Left > windowDanceResolutionRect.Left && monitor.bounds.Left < windowDanceResolutionRect.Right && monitor.bounds.Top > windowDanceResolutionRect.Top && monitor.bounds.Top < windowDanceResolutionRect.Bottom)
            {
                if (monitor.bounds.Left < result.x)
                {
                    result.x = monitor.bounds.Left;
                }

                if (monitor.bounds.Top > result.y)
                {
                    result.y = monitor.bounds.Top;
                }
            }
        }

        return result;
    }

    public override Vector2Int TranslateWindowPos(Vector2Int bottomLeftPosition, Vector2Int windowSize)
    {
        int y = WindowDanceResolution.y - bottomLeftPosition.y - windowSize.y;
        return GetBottomLeftDesktopPoint() + new Vector2Int(bottomLeftPosition.x, y);
    }
}