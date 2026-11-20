#include "ui/PreviewWindow.h"

#include "app/AppContext.h"
#include "app/Constants.h"
#include "core/WindowActions.h"

namespace wcf {
namespace {

// 该颜色区域完全透明且鼠标穿透
const COLORREF kPreviewKey = RGB(1, 2, 3);

RECT g_previewMonitorRect = { 0, 0, 0, 0 };
RECT g_previewTargetRect = { 0, 0, 0, 0 };

LRESULT CALLBACK PreviewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH keyBrush = CreateSolidBrush(kPreviewKey);
        FillRect(hdc, &rc, keyBrush);
        DeleteObject(keyBrush);

        RECT r;
        r.left = g_previewTargetRect.left - g_previewMonitorRect.left;
        r.top = g_previewTargetRect.top - g_previewMonitorRect.top;
        r.right = g_previewTargetRect.right - g_previewMonitorRect.left;
        r.bottom = g_previewTargetRect.bottom - g_previewMonitorRect.top;

        HPEN pen = CreatePen(PS_DASH, 1, RGB(0, 103, 192));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        SetBkMode(hdc, TRANSPARENT);

        // 画两圈细虚线，视觉上接近 2px
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        Rectangle(hdc, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1);

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void EnsurePreviewWindow()
{
    if (g_previewWindow) {
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = PreviewProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = WCF_PREVIEW_WINDOW_CLASS;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        classRegistered = true;
    }

    g_previewWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        WCF_PREVIEW_WINDOW_CLASS, L"", WS_POPUP,
        0, 0, 10, 10, NULL, NULL, GetModuleHandleW(NULL), NULL);

    if (g_previewWindow) {
        SetLayeredWindowAttributes(g_previewWindow, kPreviewKey, 0, LWA_COLORKEY);
    }
}

} // namespace

void ShowRegulatePreview(HWND hSettings)
{
    if (!hSettings || !IsWindowVisible(hSettings)) {
        return;   // 窗口还在 WM_CREATE 阶段时不弹预览
    }

    EnsurePreviewWindow();
    if (!g_previewWindow) {
        return;
    }

    HMONITOR hMon = MonitorFromWindow(hSettings, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
        return;
    }

    RECT target;
    if (!ComputeRegulatedRect(hSettings, &target)) {
        return;
    }

    g_previewMonitorRect = mi.rcMonitor;
    g_previewTargetRect = target;

    SetWindowPos(g_previewWindow, HWND_TOPMOST,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_previewWindow, NULL, TRUE);

    SetTimer(hSettings, 1, 500, NULL);
}

void HideRegulatePreview()
{
    if (g_previewWindow) {
        ShowWindow(g_previewWindow, SW_HIDE);
    }
}

void DestroyPreviewWindow()
{
    if (g_previewWindow) {
        DestroyWindow(g_previewWindow);
        g_previewWindow = NULL;
    }
}

} // namespace wcf
