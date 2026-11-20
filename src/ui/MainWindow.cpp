#include "ui/MainWindow.h"

#include "app/AppContext.h"
#include "app/Constants.h"
#include "core/HotkeyManager.h"
#include "core/WindowActions.h"
#include "resource.h"
#include "ui/SettingsWindow.h"
#include "ui/TrayIcon.h"

namespace wcf {
namespace {

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_HOTKEY:
    {
        HotkeyAction action;
        if (HotkeyActionFromId((int)wParam, &action)) {
            PerformHotkeyAction(action);
        }
        break;
    }

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            TrayIcon::ShowContextMenu(hwnd);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_SETTINGS:
            ShowSettingsWindow();
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

} // namespace

bool InitializeMainWindow(HINSTANCE hInstance)
{
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WCF_MAIN_WINDOW_CLASS;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_WINCLEARFOCUS));

    if (!RegisterClassW(&wc)) {
        return false;
    }

    // 隐藏的主窗口：只作为热键与托盘消息的接收者
    g_mainWindow = CreateWindowW(
        WCF_MAIN_WINDOW_CLASS,
        L"WinClearFocus",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL, NULL, hInstance, NULL);

    return g_mainWindow != NULL;
}

} // namespace wcf
