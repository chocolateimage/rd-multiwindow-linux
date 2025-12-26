using System;
using System.Collections.Generic;
using System.Linq;
using MultiWindow;
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

    public int GetMonitorIndex(IEnumerable<(Monitor monitor, int index)> list, Vector2Int playerCenter)
    {
        var match = list.FirstOrDefault((x) => x.monitor.bounds.ContainsPoint(playerCenter));

        return match == default ? 0 : match.index; 
    }

    public override List<Monitor> GetMonitors()
    {
        var monitors = Native.GetMonitors().ToList().OrderBy(monitor => monitor.X).Select((monitor, index) => (monitor: monitor.ToMonitor(), index));
        var playerPos = PlayerWindow.Instance.GetPosition();
        var playerSize = PlayerWindow.Instance.GetSize();
        var playerCenter = playerPos + (playerSize.ToVector2Int() / 2);

        // "currentMonitorIndex" seems like the primary monitor.
        currentMonitorIndex = GetMonitorIndex(monitors, playerCenter);

        var primaryMonitor = monitors.ElementAt(currentMonitorIndex);
        int primaryScale = primaryMonitor.monitor.scale;
        monitors = monitors.Where((x) => x.monitor.scale == primaryScale);

        if (RDC.windowMovement == WindowMovement.OneScreen)
        {
            currentMonitorIndex = 0;
            return [primaryMonitor.monitor];
        }

        currentMonitorIndex = GetMonitorIndex(monitors, playerCenter);

        return monitors.Select(x => x.monitor).ToList();
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