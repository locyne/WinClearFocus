#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <shellapi.h>

#include "app/AppContext.h"
#include "app/Constants.h"
#include "core/ConfigStore.h"
#include "core/HotkeyManager.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include "ui/StartupDialog.h"
#include "ui/TrayIcon.h"

namespace {

// 命令行是否包含 -silent（大小写不敏感，兼容 /silent、--silent）
bool HasSilentSwitch()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return false;
    }

    bool silent = false;
    for (int i = 1; i < argc; ++i) {
        if (lstrcmpiW(argv[i], L"-silent") == 0 ||
            lstrcmpiW(argv[i], L"--silent") == 0 ||
            lstrcmpiW(argv[i], L"/silent") == 0) {
            silent = true;
            break;
        }
    }

    LocalFree(argv);
    return silent;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // 初始化通用控件（Win11 视觉样式由 exe 内嵌的 comctl32 v6 manifest 启用）
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // 命令行 -silent 优先级最高：命中后无论配置如何都不弹任何启动提示
    const bool forceSilent = HasSilentSwitch();

    // 检查是否已有实例运行
    if (FindWindowW(WCF_MAIN_WINDOW_CLASS, NULL)) {
        MessageBoxW(NULL, L"WinClearFocus 已在运行中！", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 初始化 GDI+（设置界面自绘用），退出前释放
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL) != Gdiplus::Ok) {
        gdiplusToken = 0;
    }

    // 加载配置（首次运行会生成 config.ini）
    wcf::LoadConfig();

    // 创建隐藏的主窗口
    if (!wcf::InitializeMainWindow(hInstance)) {
        MessageBoxW(NULL, L"注册窗口类或创建主窗口失败！", L"错误", MB_OK | MB_ICONERROR);
        if (gdiplusToken) {
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }
        return 1;
    }

    // 创建系统托盘图标并注册全局热键
    wcf::TrayIcon::Create(wcf::g_mainWindow);
    wcf::RegisterHotkeys(wcf::g_mainWindow);

    // 显示启动提示：命令行 -silent 绝对静默；否则由 config.ini 决定。
    // 使用原生对话框，标题栏显示程序图标 + 英文名 + 版本。
    if (!forceSilent && wcf::Settings().showStartupMessage != 0) {
        wcf::ShowStartupDialog(hInstance, wcf::g_mainWindow);
    }

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 清理资源
    wcf::UnregisterHotkeys(wcf::g_mainWindow);
    wcf::TrayIcon::Remove();

    if (gdiplusToken) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }

    return (int)msg.wParam;
}
