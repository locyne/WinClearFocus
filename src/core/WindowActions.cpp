#include "core/WindowActions.h"

#include "app/AppContext.h"
#include "util/Win32Utils.h"

#include <cstdlib>

namespace wcf {

HWND GetTopMostWindow()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        hwnd = GetWindow(GetDesktopWindow(), GW_CHILD);
    }
    return hwnd;
}

void SetWindowTransparency(HWND hwnd, int alpha)
{
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (alpha < 255) {
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA);
    } else {
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    }
}

int GetWindowTransparency(HWND hwnd)
{
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED) {
        BYTE alpha = 255;
        COLORREF colorKey = 0;
        DWORD flags = 0;
        if (GetLayeredWindowAttributes(hwnd, &colorKey, &alpha, &flags)) {
            return (int)alpha;
        }
    }
    return 255;   // 默认完全不透明
}

void AdjustWindowTransparency(bool increase)
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || IsOwnWindow(hwnd)) {
        return;
    }

    const int currentAlpha = GetWindowTransparency(hwnd);
    const int step = 255 * Settings().transparencyStep / 100;
    int newAlpha;

    if (increase) {
        // 增加透明度（减少 alpha 值），最低到 1：近乎全透明但仍可用“重置”恢复
        newAlpha = currentAlpha - step;
        if (newAlpha < 1) newAlpha = 1;
    } else {
        // 减少透明度（增加 alpha 值），最高 255 = 完全不透明
        newAlpha = currentAlpha + step;
        if (newAlpha > 255) newAlpha = 255;
    }

    SetWindowTransparency(hwnd, newAlpha);
}

void ResetWindowTransparency()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || IsOwnWindow(hwnd)) {
        return;
    }

    SetWindowTransparency(hwnd, 255);
}

void CenterWindow()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || IsOwnWindow(hwnd)) {
        return;
    }

    // 最大化窗口已经铺满工作区，再“居中”反而会把它挪出工作区、压到任务栏下面
    if (IsZoomed(hwnd)) {
        return;
    }

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    const int windowWidth = rect.right - rect.left;
    const int windowHeight = rect.bottom - rect.top;

    // 目标窗口所在显示器；跨屏或最小化时取最近的一台，避免“飞回主屏”
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
        return;
    }

    // rcWork 天然扣除了任务栏（以及停靠的 AppBar），rcMonitor 是整块屏幕
    const RECT& workArea = Settings().centerIgnoreTaskbar ? mi.rcWork : mi.rcMonitor;
    const int workWidth = workArea.right - workArea.left;
    const int workHeight = workArea.bottom - workArea.top;

    const int x = workArea.left + (workWidth - windowWidth) / 2;
    const int y = workArea.top + (workHeight - windowHeight) / 2;

    // SWP_NOACTIVATE：只移动不激活，避免抢走用户当前窗口的焦点
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void ShakeWindow()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || IsOwnWindow(hwnd)) {
        return;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    const int originalX = rect.left;
    const int originalY = rect.top;

    const int shakeDistance = 5;
    const int shakeTimes = 6;
    const int shakeDelay = 50;   // 毫秒

    for (int i = 0; i < shakeTimes; ++i) {
        const int offsetX = (i % 2 == 0) ? shakeDistance : -shakeDistance;
        const int offsetY = (i % 4 < 2) ? shakeDistance : -shakeDistance;

        SetWindowPos(hwnd, NULL, originalX + offsetX, originalY + offsetY,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        Sleep(shakeDelay);
    }

    SetWindowPos(hwnd, NULL, originalX, originalY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void SendWindowToBottom()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || IsOwnWindow(hwnd)) {
        return;
    }

    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool IsMaximizedOrFullscreen(HWND hwnd)
{
    if (IsZoomed(hwnd)) {
        return true;
    }

    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
        return false;
    }

    RECT wr;
    if (!GetWindowRect(hwnd, &wr)) {
        return false;
    }

    const RECT& m = mi.rcMonitor;
    const int tol = 2;
    return std::abs(wr.left - m.left) <= tol && std::abs(wr.top - m.top) <= tol &&
           std::abs(wr.right - m.right) <= tol && std::abs(wr.bottom - m.bottom) <= tol;
}

bool ComputeRegulatedRect(HWND hwnd, RECT* out)
{
    if (!out) {
        return false;
    }

    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
        return false;
    }

    const AppSettings& s = Settings();

    // 尺寸基准：百分比模式看“缩放时忽略任务栏”；
    // 固定尺寸始终以工作区为上限，避免目标尺寸超过任务栏上方的可用空间。
    const bool sizeUseWorkArea = (s.regulateMode == 0) ? (s.regulateIgnoreTaskbar != 0) : true;
    const RECT sizeArea = sizeUseWorkArea ? mi.rcWork : mi.rcMonitor;
    const int aw = sizeArea.right - sizeArea.left;
    const int ah = sizeArea.bottom - sizeArea.top;
    if (aw <= 0 || ah <= 0) {
        return false;
    }

    int w, h;
    if (s.regulateMode == 0) {
        const int scale = ClampInt(s.regulateScale, 30, 90);
        w = aw * scale / 100;
        h = ah * scale / 100;
    } else {
        w = ClampInt(s.regulateWidth, 100, 8000);
        h = ClampInt(s.regulateHeight, 100, 8000);
        // 固定尺寸超出当前工作区时，等比缩小到刚好放得下
        if (w > aw || h > ah) {
            const double fx = (double)aw / (double)w;
            const double fy = (double)ah / (double)h;
            const double f = (fx < fy) ? fx : fy;
            w = (int)(w * f);
            h = (int)(h * f);
        }
    }

    w = ClampInt(w, 100, 8000);
    h = ClampInt(h, 100, 8000);

    int x, y;
    if (s.regulateCenter) {
        // 居中位置统一遵循“居中时忽略任务栏”，与 Alt+Shift+C 的行为一致；
        // 这样切换该选项时，虚线预览和实际调整结果都会同步移动。
        const RECT centerArea = s.centerIgnoreTaskbar ? mi.rcWork : mi.rcMonitor;
        const int cw = centerArea.right - centerArea.left;
        const int ch = centerArea.bottom - centerArea.top;
        x = centerArea.left + (cw - w) / 2;
        y = centerArea.top + (ch - h) / 2;
    } else {
        // 不居中：尺寸变化但保持原窗口左上角不动
        RECT wr;
        GetWindowRect(hwnd, &wr);
        x = wr.left;
        y = wr.top;
    }

    out->left = x;
    out->top = y;
    out->right = x + w;
    out->bottom = y + h;
    return true;
}

void RegulateWindowSize()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || IsOwnWindow(hwnd)) {
        return;
    }
    if (IsMaximizedOrFullscreen(hwnd)) {
        return;   // 最大化 / 全屏窗口直接忽略
    }

    RECT target;
    if (!ComputeRegulatedRect(hwnd, &target)) {
        return;
    }

    SetWindowPos(hwnd, NULL, target.left, target.top,
                 target.right - target.left, target.bottom - target.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void PerformHotkeyAction(HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::TransparencyUp:
        AdjustWindowTransparency(true);
        break;
    case HotkeyAction::TransparencyDown:
        AdjustWindowTransparency(false);
        break;
    case HotkeyAction::CenterWindow:
        CenterWindow();
        break;
    case HotkeyAction::ShakeWindow:
        ShakeWindow();
        break;
    case HotkeyAction::ResetTransparency:
        ResetWindowTransparency();
        break;
    case HotkeyAction::SendToBottom:
        SendWindowToBottom();
        break;
    case HotkeyAction::Regulate:
        RegulateWindowSize();
        break;
    case HotkeyAction::Count:
        break;
    }
}

} // namespace wcf
