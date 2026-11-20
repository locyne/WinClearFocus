#include "ui/TrayIcon.h"

#include <shellapi.h>
#include <stdio.h>

#include "app/Constants.h"
#include "resource.h"

namespace wcf {
namespace TrayIcon {
namespace {

NOTIFYICONDATAW g_nid = { 0 };

} // namespace

void Create(HWND owner)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = owner;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    g_nid.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_WINCLEARFOCUS));
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    }

    swprintf_s(g_nid.szTip, sizeof(g_nid.szTip) / sizeof(wchar_t),
               L"WinClearFocus %s - 窗口透明度控制", WCF_APP_VERSION);

    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void Recreate(HWND owner)
{
    // 先清掉旧记录再重新添加；explorer 重启后旧图标已经不存在，
    // 重复 NIM_DELETE 也是安全的。
    if (g_nid.cbSize != 0) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
    }
    Create(owner);
}

void Remove()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowContextMenu(HWND owner)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, owner, NULL);
    DestroyMenu(menu);
}

} // namespace TrayIcon
} // namespace wcf
