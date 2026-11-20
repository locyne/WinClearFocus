#include "app/Application.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "app/AppContext.h"
#include "core/HotkeyManager.h"
#include "ui/TrayIcon.h"

namespace wcf {

bool IsRunningAsAdmin()
{
    return IsUserAnAdmin() != FALSE;
}

void RestartAsAdmin()
{
    wchar_t exePath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        return;
    }

    // 先彻底退出当前实例：否则新进程的 FindWindow 会判定“已在运行中”而直接退出
    TrayIcon::Remove();
    UnregisterHotkeys(g_mainWindow);
    if (g_settingsWindow) {
        DestroyWindow(g_settingsWindow);
        g_settingsWindow = NULL;
    }
    if (g_mainWindow) {
        DestroyWindow(g_mainWindow);
        g_mainWindow = NULL;
    }

    // runas 会触发 UAC 对话框；lpParameters 传 NULL，新进程不带参数
    HINSTANCE result = ShellExecuteW(NULL, L"runas", exePath, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        // 用户取消 UAC 或提权失败（ERROR_CANCELLED = 1223）
        MessageBoxW(NULL, L"提权未完成：用户取消了 UAC 授权，或系统拒绝提权。",
                    L"WinClearFocus", MB_OK | MB_ICONWARNING);
    }

    PostQuitMessage(0);
}

} // namespace wcf
