#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>       // IsUserAnAdmin：判断当前进程是否以管理员运行
#include <commctrl.h>
#include <windowsx.h>     // GET_X_LPARAM 等
#include <gdiplus.h>      // 设置界面自绘（现代风格）
#include <dwmapi.h>       // 窗口圆角 / DWM 属性
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

// 常量定义
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SETTINGS 1002
#define ID_HOTKEY_TRANSPARENCY_UP 1
#define ID_HOTKEY_TRANSPARENCY_DOWN 2
#define ID_HOTKEY_CENTER_WINDOW 3
#define ID_HOTKEY_SHAKE_WINDOW 4
#define ID_HOTKEY_RESET_TRANSPARENCY 5
#define ID_HOTKEY_SEND_TO_BOTTOM 6
#define MAX_PATH_LEN 260
#define APP_VERSION L"v1.0.0"

// 控件ID
#define IDC_TRANSPARENCY_UP_BUTTON 2001
#define IDC_TRANSPARENCY_DOWN_BUTTON 2002
#define IDC_CENTER_BUTTON 2003
#define IDC_SHAKE_BUTTON 2004
#define IDC_TRANSPARENCY_SLIDER 2005
#define IDC_SAVE_BUTTON 2006
#define IDC_TRANSPARENCY_LABEL 2007
#define IDC_TRANSPARENCY_UP_DISPLAY 2008
#define IDC_TRANSPARENCY_DOWN_DISPLAY 2009
#define IDC_CENTER_DISPLAY 2010
#define IDC_SHAKE_DISPLAY 2011
#define IDC_TRANSPARENCY_ENABLE 2012
#define IDC_CENTER_ENABLE 2013
#define IDC_SHAKE_ENABLE 2014
#define IDC_SHOW_STARTUP_MSG 2015
#define IDC_ADMIN_STATUS 2016
#define IDC_ELEVATE 2017
#define IDC_PERMISSION_HELP 2018
#define IDC_CENTER_IGNORE_TASKBAR 2019
#define IDC_RESET_TRANSPARENCY_DISPLAY 2020
#define IDC_RESET_TRANSPARENCY_BUTTON 2021
#define IDC_SEND_TO_BOTTOM_ENABLE 2022
#define IDC_SEND_TO_BOTTOM_DISPLAY 2023
#define IDC_SEND_TO_BOTTOM_BUTTON 2024
#define IDC_AUTOSTART 2025
#define IDC_AUTOSTART_DELAY 2026

// 全局变量
HWND g_hMainWnd = NULL;           // 主窗口句柄
HWND g_hSettingsWnd = NULL;       // 设置窗口句柄
NOTIFYICONDATA g_nid = { 0 };     // 系统托盘图标数据
int g_transparencyStep = 10;      // 透明度调整步长（默认10%）

// 启动提示开关：1=启动后弹提示框，0=静默进托盘（对应 config.ini 的 [Settings] ShowStartupMessage）
int g_showStartupMessage = 1;

// 配置文件绝对路径：exe 所在目录下的 config.ini。
// 不能用相对路径 ".\config.ini"：计划任务在登录时以管理员身份启动，
// 工作目录往往是 C:\Windows\System32，会导致配置读取/保存全部失效。
wchar_t g_configPath[MAX_PATH] = { 0 };

// 开机自启的延迟秒数（0/5/10/15/30，0 = 不延迟）
int g_autoStartDelay = 0;

// 窗口居中时是否忽略任务栏：
// 1 = 以显示器工作区 rcWork 居中（默认，不会被任务栏挡住）
// 0 = 以整个显示器 rcMonitor 居中
int g_centerIgnoreTaskbar = 1;

// 权限说明 Tooltip 的文本。
// 必须是静态存储：TTM_ADDTOOL 只保存指针，不会拷贝字符串内容。
static wchar_t g_permissionTipText[] =
    L"提示：部分软件是以管理员权限运行的，普通权限下的本程序无法控制这类软件。"
    L"若需控制它们，请点击“去提权”以管理员身份重启本程序。";

// 透明度热键设置（默认 Alt+Shift+Right / Alt+Shift+Left，默认不启用）
UINT g_transparencyUpModifiers = MOD_ALT | MOD_SHIFT;    // 透明度增加修饰键
UINT g_transparencyUpKey = VK_RIGHT;                     // 透明度增加按键
UINT g_transparencyDownModifiers = MOD_ALT | MOD_SHIFT;  // 透明度减少修饰键
UINT g_transparencyDownKey = VK_LEFT;                    // 透明度减少按键
BOOL g_enableTransparencyUp = FALSE;                     // 是否启用透明度增加功能
BOOL g_enableTransparencyDown = FALSE;                   // 是否启用透明度减少功能

// 窗口居中热键设置（默认 Alt+Shift+C，默认启用）
UINT g_centerModifiers = MOD_ALT | MOD_SHIFT;            // 窗口居中修饰键
UINT g_centerKey = 'C';                                  // 窗口居中按键
BOOL g_enableCenter = TRUE;                              // 是否启用窗口居中功能

// 窗口抖动热键设置（默认 Alt+Shift+Down，默认不启用）
UINT g_shakeModifiers = MOD_ALT | MOD_SHIFT;             // 窗口抖动修饰键
UINT g_shakeKey = VK_DOWN;                               // 窗口抖动按键
BOOL g_enableShake = FALSE;                              // 是否启用窗口抖动功能

// 重置透明度热键设置（默认 Alt+Shift+Up，默认启用）—— 用来救回被调到近乎全透明的窗口
UINT g_resetTransparencyModifiers = MOD_ALT | MOD_SHIFT; // 重置透明度修饰键
UINT g_resetTransparencyKey = VK_UP;                     // 重置透明度按键
BOOL g_enableResetTransparency = TRUE;                   // 是否启用重置透明度功能

// 窗口置底热键设置（默认 Alt+Shift+Z，默认不启用）
UINT g_sendToBottomModifiers = MOD_ALT | MOD_SHIFT;      // 窗口置底修饰键
UINT g_sendToBottomKey = 'Z';                            // 窗口置底按键
BOOL g_enableSendToBottom = FALSE;                       // 是否启用窗口置底功能

// 热键监听状态
BOOL g_isListeningHotkey = FALSE;     // 是否正在监听热键输入
int g_currentListeningType = 0;       // 当前监听类型: 0=无, 1=透明度增加, 2=透明度减少, 3=居中, 4=抖动
HWND g_hCurrentButton = NULL;         // 当前正在设置的按钮句柄
HWND g_hCurrentDisplay = NULL;        // 当前正在设置的显示框句柄
DWORD g_listeningStartTime = 0;       // 监听开始时间（用于超时检测）

// 函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);         // 主窗口消息处理函数
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);       // 设置窗口消息处理函数
void CreateTrayIcon(HWND hwnd);                   // 创建系统托盘图标
void RemoveTrayIcon();                            // 移除系统托盘图标
void ShowContextMenu(HWND hwnd);                  // 显示右键菜单
void ShowSettingsWindow();                        // 显示设置窗口
void RegisterHotKeys(HWND hwnd);                  // 注册全局热键
void UnregisterHotKeys(HWND hwnd);                // 注销全局热键
void AdjustWindowTransparency(BOOL increase);     // 调整窗口透明度
void CenterWindow();                              // 将窗口居中显示
void ShakeWindow();                               // 窗口抖动效果
void ResetWindowTransparency();                   // 重置窗口透明度（恢复不透明）
void SendWindowToBottom();                        // 将窗口沉到最底层
void LoadConfig();                                // 从配置文件加载设置
void SaveConfig();                                // 保存设置到配置文件
HWND GetTopMostWindow();                          // 获取最上层窗口
void SetWindowTransparency(HWND hwnd, int alpha); // 设置窗口透明度
int GetWindowTransparency(HWND hwnd);             // 获取窗口透明度
wchar_t* GetModifierName(UINT modifiers);         // 获取修饰键名称
wchar_t* GetKeyName(UINT vkCode);                 // 获取按键名称
bool HasSilentSwitch();                           // 命令行是否带 -silent
void RestartAsAdmin();                            // 以管理员身份重新启动自己

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

// 以管理员身份重新启动自己（UAC 提权）
void RestartAsAdmin()
{
    wchar_t exePath[MAX_PATH] = { 0 };
    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        return;
    }

    // 先彻底退出当前实例：否则新进程的 FindWindow 会判定“已在运行中”而直接退出
    RemoveTrayIcon();
    UnregisterHotKeys(g_hMainWnd);
    if (g_hSettingsWnd) {
        DestroyWindow(g_hSettingsWnd);
        g_hSettingsWnd = NULL;
    }
    if (g_hMainWnd) {
        DestroyWindow(g_hMainWnd);
        g_hMainWnd = NULL;
    }

    // runas 会触发 UAC 对话框；lpParameters 传 NULL，保持新进程使用相同的启动参数策略（这里不带参数）
    HINSTANCE result = ShellExecute(NULL, L"runas", exePath, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        // 用户取消 UAC 或提权失败（ERROR_CANCELLED = 1223）
        MessageBox(NULL, L"提权未完成：用户取消了 UAC 授权，或系统拒绝提权。", L"WinClearFocus", MB_OK | MB_ICONWARNING);
    }

    PostQuitMessage(0);
}

// 计算配置文件绝对路径：<exe 所在目录>\config.ini
static void GetConfigPath()
{
    wchar_t exePath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        // 极端情况下退回当前目录
        wcscpy_s(g_configPath, MAX_PATH, L"config.ini");
        return;
    }

    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) {
        *(slash + 1) = L'\0';   // 保留目录部分（含末尾反斜杠）
    } else {
        exePath[0] = L'\0';
    }

    swprintf_s(g_configPath, MAX_PATH, L"%sconfig.ini", exePath);
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 最先确定配置文件路径：后面 LoadConfig() 和所有保存都依赖它
    GetConfigPath();

    // 初始化通用控件（Win11 视觉样式由 exe 内嵌的 comctl32 v6 manifest 启用）
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // 命令行 -silent 优先级最高：命中后无论配置如何都不弹任何启动提示
    bool forceSilent = HasSilentSwitch();

    // 检查是否已有实例运行
    HWND existingWnd = FindWindow(L"WinClearFocusClass", NULL);
    if (existingWnd) {
        MessageBox(NULL, L"WinClearFocus 已在运行中！", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 初始化 GDI+（设置界面自绘用），退出前释放
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL) != Gdiplus::Ok) {
        gdiplusToken = 0;
    }

    // 注册窗口类
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WinClearFocusClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINCLEARFOCUS));

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"注册窗口类失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 加载配置
    LoadConfig();

    // 创建隐藏的主窗口
    g_hMainWnd = CreateWindow(
        L"WinClearFocusClass",
        L"WinClearFocus",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hMainWnd) {
        MessageBox(NULL, L"创建主窗口失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建系统托盘图标
    CreateTrayIcon(g_hMainWnd);

    // 注册全局热键
    RegisterHotKeys(g_hMainWnd);

    // 显示启动提示：命令行 -silent 绝对静默；否则由 config.ini 的 ShowStartupMessage 决定
    if (!forceSilent && g_showStartupMessage != 0) {
        wchar_t startupMsg[600];
        swprintf_s(startupMsg, 600, L"WinClearFocus %s 已启动！\n\n默认热键：\n- Alt+Shift+C 窗口居中\n- Alt+Shift+↓ 窗口抖动（默认关闭）\n- Alt+Shift+←/→ 调整窗口透明度（默认关闭）\n- Alt+Shift+↑ 重置透明度\n- Alt+Shift+Z 窗口置底（默认关闭）\n\n右键点击托盘图标进行设置\n\n提示：命令行加 -silent 可静默启动", APP_VERSION);
        MessageBox(NULL, startupMsg, L"WinClearFocus 启动成功", MB_OK | MB_ICONINFORMATION);
    }

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    UnregisterHotKeys(g_hMainWnd);
    RemoveTrayIcon();

    if (gdiplusToken) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }

    return (int)msg.wParam;
}

// 主窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        break;

    case WM_HOTKEY:
        switch (wParam) {
        case ID_HOTKEY_TRANSPARENCY_UP:
            AdjustWindowTransparency(TRUE); // 增加透明度（减少不透明度）
            break;
        case ID_HOTKEY_TRANSPARENCY_DOWN:
            AdjustWindowTransparency(FALSE); // 减少透明度（增加不透明度）
            break;
        case ID_HOTKEY_CENTER_WINDOW:
            CenterWindow(); // 窗口居中
            break;
        case ID_HOTKEY_SHAKE_WINDOW:
            ShakeWindow(); // 窗口抖动
            break;
        case ID_HOTKEY_RESET_TRANSPARENCY:
            ResetWindowTransparency(); // 重置透明度
            break;
        case ID_HOTKEY_SEND_TO_BOTTOM:
            SendWindowToBottom(); // 窗口置底
            break;
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
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
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// ==================== 开机自启 ====================

#define AUTOSTART_RUN_KEY    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTOSTART_RUN_VALUE  L"WinClearFocus"
#define AUTOSTART_TASK_PATH  L"\\WinClearFocus\\WinClearFocus"

// 取系统目录下 schtasks.exe 的完整路径：不要按 PATH 搜索，
// 否则程序目录里放一个同名 exe 就能劫持我们（尤其当程序以管理员运行时）
static const wchar_t* SchtasksPath()
{
    static wchar_t path[MAX_PATH] = { 0 };
    if (!path[0]) {
        UINT len = GetSystemDirectoryW(path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            wcscpy_s(path, MAX_PATH, L"C:\\Windows\\System32\\schtasks.exe");
        } else {
            wcscat_s(path, MAX_PATH, L"\\schtasks.exe");
        }
    }
    return path;
}

// 运行命令行（窗口隐藏、输出丢弃），返回退出码是否为 0
static bool RunProcess(const wchar_t* cmdLine, DWORD* exitCode)
{
    wchar_t buffer[2048];
    wcscpy_s(buffer, 2048, cmdLine);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = hNul;
    si.hStdError = hNul;

    PROCESS_INFORMATION pi = { 0 };
    BOOL started = CreateProcessW(NULL, buffer, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!started) {
        if (hNul) CloseHandle(hNul);
        if (exitCode) *exitCode = GetLastError();
        return false;
    }

    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (hNul) CloseHandle(hNul);

    if (exitCode) *exitCode = code;
    return code == 0;
}

// HKCU\...\Run 里是否已有本程序的启动项
static bool RunValueExists()
{
    wchar_t value[MAX_PATH + 32] = { 0 };
    DWORD size = sizeof(value);
    LONG r = RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, AUTOSTART_RUN_VALUE,
                          RRF_RT_REG_SZ, NULL, value, &size);
    return r == ERROR_SUCCESS;
}

// 写入/删除注册表启动项，值形如 "C:\path\WinClearFocus.exe" -silent
static bool SetRunValue(bool enable)
{
    HKEY key = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, NULL) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = true;
    if (enable) {
        wchar_t exePath[MAX_PATH] = { 0 };
        if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
            ok = false;
        } else {
            wchar_t value[MAX_PATH + 32];
            swprintf_s(value, MAX_PATH + 32, L"\"%s\" -silent", exePath);
            ok = RegSetValueExW(key, AUTOSTART_RUN_VALUE, 0, REG_SZ, (const BYTE*)value,
                                (DWORD)((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        }
    } else {
        LONG r = RegDeleteValueW(key, AUTOSTART_RUN_VALUE);
        ok = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(key);
    return ok;
}

// 计划任务 \WinClearFocus\WinClearFocus 是否存在
static bool TaskExists()
{
    wchar_t cmd[512];
    swprintf_s(cmd, 512, L"%s /query /tn \"%s\"", SchtasksPath(), AUTOSTART_TASK_PATH);
    DWORD code = 1;
    RunProcess(cmd, &code);
    return code == 0;
}

// 创建计划任务：登录时（可延迟）以最高权限启动 "<exe>" -silent
static bool CreateScheduledTask(int delaySeconds)
{
    wchar_t exePath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        return false;
    }

    wchar_t cmd[2048];
    if (delaySeconds > 0) {
        // /tr 的值本身含引号，需要写成 \" 才能穿过命令行解析；
        // /delay 的格式是 mmmm:ss（分钟:秒），15 秒 => 0000:15
        swprintf_s(cmd, 2048,
            L"%s /create /tn \"%s\" /tr \"\\\"%s\\\" -silent\" /sc onlogon /rl highest /delay %04d:%02d /f",
            SchtasksPath(), AUTOSTART_TASK_PATH, exePath, delaySeconds / 60, delaySeconds % 60);
    } else {
        swprintf_s(cmd, 2048,
            L"%s /create /tn \"%s\" /tr \"\\\"%s\\\" -silent\" /sc onlogon /rl highest /f",
            SchtasksPath(), AUTOSTART_TASK_PATH, exePath);
    }

    DWORD code = 1;
    return RunProcess(cmd, &code);
}

// 删除计划任务
static bool DeleteScheduledTask()
{
    wchar_t cmd[512];
    swprintf_s(cmd, 512, L"%s /delete /tn \"%s\" /f", SchtasksPath(), AUTOSTART_TASK_PATH);
    DWORD code = 1;
    RunProcess(cmd, &code);
    return code == 0;
}

// 启动项是否已存在（注册表或计划任务任一）
static bool IsAutoStartRegistered()
{
    if (RunValueExists()) {
        return true;
    }
    return TaskExists();
}

// 应用开机自启设置；detail 返回给用户看的说明
static bool ApplyAutoStart(BOOL enable, int delaySeconds, wchar_t* detail, size_t detailLen)
{
    if (!enable) {
        bool runOk = SetRunValue(false);
        bool taskOk = true;
        if (TaskExists()) {
            taskOk = DeleteScheduledTask();
        }
        if (!runOk) {
            swprintf_s(detail, detailLen, L"移除注册表启动项失败，请检查系统权限。");
            return false;
        }
        if (!taskOk) {
            swprintf_s(detail, detailLen, L"已移除注册表启动项，但计划任务 %s 需要管理员权限才能删除。", AUTOSTART_TASK_PATH);
            return false;
        }
        swprintf_s(detail, detailLen, L"已关闭（注册表启动项和计划任务均已清理）。");
        return true;
    }

    if (!IsUserAnAdmin()) {
        // 普通用户：只能写 HKCU Run，无法设置延迟
        if (!SetRunValue(true)) {
            swprintf_s(detail, detailLen, L"写入注册表 HKCU\\...\\Run 失败（错误码 %lu）。", GetLastError());
            return false;
        }
        // 如果以前用管理员建过计划任务，尽力清掉，避免双重启动
        bool taskRemoved = true;
        if (TaskExists()) {
            taskRemoved = DeleteScheduledTask();
        }
        if (!taskRemoved) {
            swprintf_s(detail, detailLen,
                L"已写入注册表 HKCU\\...\\Run，但检测到已有计划任务 %s 且无权限删除，登录时可能重复启动；建议以管理员身份重新设置。",
                AUTOSTART_TASK_PATH);
            return true;
        }
        swprintf_s(detail, detailLen, L"已启用（注册表 HKCU\\...\\Run，普通用户权限不支持延迟启动）。");
        return true;
    }

    // 管理员：先无条件删除可能残留的同名任务，再用统一参数重建一个干净的。
    // 任务不存在时 schtasks /delete 会返回非 0，这里刻意忽略返回值（静默清理，不打扰用户）
    DeleteScheduledTask();

    // 管理员：创建登录时运行、最高权限的计划任务
    if (!CreateScheduledTask(delaySeconds)) {
        swprintf_s(detail, detailLen, L"创建计划任务 %s 失败。请确认程序以管理员身份运行，且系统允许创建计划任务。", AUTOSTART_TASK_PATH);
        return false;
    }

    // 防止双重启动：计划任务建好后删掉旧的注册表启动项
    SetRunValue(false);

    if (delaySeconds > 0) {
        swprintf_s(detail, detailLen, L"已启用（计划任务 %s，登录后延迟 %d 秒，最高权限）。",
                   AUTOSTART_TASK_PATH, delaySeconds);
    } else {
        swprintf_s(detail, detailLen, L"已启用（计划任务 %s，登录时启动，最高权限）。", AUTOSTART_TASK_PATH);
    }
    return true;
}

// 下拉框当前选中的延迟秒数
static int ComboAutoStartDelay(HWND combo)
{
    static const int kDelays[] = { 0, 5, 10, 15, 30 };
    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)(sizeof(kDelays) / sizeof(kDelays[0]))) {
        return 0;
    }
    return kDelays[sel];
}

// 只有“勾选 + 管理员”才允许选延迟
static void UpdateAutoStartEnabledState(HWND check, HWND combo)
{
    BOOL on = (SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(combo, on && IsUserAnAdmin() != FALSE);
}

// ==================== 设置界面外观（GDI+ / DWM） ====================

static const int kTitleBarHeight = 36;
static const int kSettingsWidth  = 460;
static const int kSettingsHeight = 710;          // 内容 + 自绘标题栏 + 加大后的块间距
static const COLORREF kSettingsBg = RGB(243, 243, 243);

static bool g_hoverClose = false;
static bool g_hoverSave = false;
static bool g_hoverCancel = false;

static HBRUSH SettingsBgBrush()
{
    static HBRUSH brush = CreateSolidBrush(kSettingsBg);
    return brush;
}

static RECT CloseButtonRect(int clientWidth)
{
    RECT r = { clientWidth - 40, 6, clientWidth - 8, 38 };
    return r;
}

static RECT SaveButtonRect(int clientWidth, int clientHeight)
{
    RECT r = { clientWidth - 16 - 108, clientHeight - 16 - 36, clientWidth - 16, clientHeight - 16 };
    return r;
}

static RECT CancelButtonRect(int clientWidth, int clientHeight)
{
    RECT r = SaveButtonRect(clientWidth, clientHeight);
    r.right = r.left - 12;
    r.left = r.right - 88;
    return r;
}

static bool HitRect(const RECT& r, int x, int y)
{
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void FillRoundedRect(Gdiplus::Graphics& g, const RECT& r, int radius, const Gdiplus::Color& color)
{
    Gdiplus::GraphicsPath path;
    const int d = radius * 2;
    path.AddArc(r.left, r.top, d, d, 180.0f, 90.0f);
    path.AddArc(r.right - d, r.top, d, d, 270.0f, 90.0f);
    path.AddArc(r.right - d, r.bottom - d, d, d, 0.0f, 90.0f);
    path.AddArc(r.left, r.bottom - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
    Gdiplus::SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

static void DrawCenteredText(Gdiplus::Graphics& g, const wchar_t* text, const RECT& r,
                             Gdiplus::Font& font, const Gdiplus::Color& color)
{
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::RectF box((Gdiplus::REAL)r.left, (Gdiplus::REAL)r.top,
                       (Gdiplus::REAL)(r.right - r.left), (Gdiplus::REAL)(r.bottom - r.top));
    g.DrawString(text, -1, &font, box, &fmt, &brush);
}

// 自绘整个设置窗口（双缓冲，避免闪烁）
static void PaintSettingsWindow(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    const int w = rc.right;
    const int h = rc.bottom;
    if (w <= 0 || h <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // 背景
        Gdiplus::SolidBrush bg(Gdiplus::Color(243, 243, 243));
        g.FillRectangle(&bg, 0, 0, w, h);

        // 标题
        Gdiplus::Font titleFont(L"Microsoft YaHei UI", 17.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush titleBrush(Gdiplus::Color(32, 32, 32));
        g.DrawString(L"WinClearFocus 设置", -1, &titleFont, Gdiplus::PointF(16.0f, 8.0f), &titleBrush);

        // 关闭按钮：圆形，悬停变红
        const RECT cb = CloseButtonRect(w);
        const int cx = (cb.left + cb.right) / 2;
        const int cy = (cb.top + cb.bottom) / 2;
        const int cr = 13;
        if (g_hoverClose) {
            Gdiplus::SolidBrush red(Gdiplus::Color(196, 43, 28));
            g.FillEllipse(&red, cx - cr, cy - cr, cr * 2, cr * 2);
        }
        Gdiplus::Pen cross(g_hoverClose ? Gdiplus::Color(255, 255, 255) : Gdiplus::Color(80, 80, 80), 1.6f);
        g.DrawLine(&cross, (Gdiplus::REAL)(cx - 4), (Gdiplus::REAL)(cy - 4), (Gdiplus::REAL)(cx + 4), (Gdiplus::REAL)(cy + 4));
        g.DrawLine(&cross, (Gdiplus::REAL)(cx - 4), (Gdiplus::REAL)(cy + 4), (Gdiplus::REAL)(cx + 4), (Gdiplus::REAL)(cy - 4));

        // 底部分隔线
        Gdiplus::Pen sep(Gdiplus::Color(226, 226, 226), 1.0f);
        g.DrawLine(&sep, 0.0f, (Gdiplus::REAL)(h - 58), (Gdiplus::REAL)w, (Gdiplus::REAL)(h - 58));

        // 底部按钮（圆角，替代原生按钮）
        Gdiplus::Font btnFont(L"Microsoft YaHei UI", 15.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        const RECT sb = SaveButtonRect(w, h);
        FillRoundedRect(g, sb, 6, g_hoverSave ? Gdiplus::Color(0, 90, 168) : Gdiplus::Color(0, 103, 192));
        DrawCenteredText(g, L"保存设置", sb, btnFont, Gdiplus::Color(255, 255, 255));

        const RECT xb = CancelButtonRect(w, h);
        FillRoundedRect(g, xb, 6, g_hoverCancel ? Gdiplus::Color(214, 214, 214) : Gdiplus::Color(229, 229, 229));
        DrawCenteredText(g, L"关闭", xb, btnFont, Gdiplus::Color(32, 32, 32));
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

// 给设置窗口的所有子控件统一应用字体的回调
static BOOL CALLBACK SetChildFontProc(HWND child, LPARAM lParam)
{
    SendMessageW(child, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

// 设置窗口过程
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTransparencySlider, hTransparencyLabel;
    static HWND hTransparencyUpDisplay, hTransparencyDownDisplay, hCenterDisplay, hShakeDisplay;
    static HWND hTransparencyUpButton, hTransparencyDownButton, hCenterButton, hShakeButton;
    static HWND hTransparencyEnable, hCenterEnable, hShakeEnable;
    static HWND hShowStartupMsg, hCenterIgnoreTaskbar, hAdminStatus, hElevateButton;
    static HWND hResetTransparencyDisplay, hResetTransparencyButton;
    static HWND hSendToBottomEnable, hSendToBottomDisplay, hSendToBottomButton;
    static HWND hAutoStart, hAutoStartDelay;
    static HWND hPermissionTip = NULL;       // “权限说明”的 Tooltip 窗口
    static HICON hPermissionIcon = NULL;     // 问号图标（自建副本，需要自己销毁）
    static bool permissionIconOwned = false;
    static bool trackingMouse = false;

    switch (uMsg) {
    case WM_NCCALCSIZE:
        if (wParam == TRUE) {
            return 0;   // 客户区覆盖整个窗口：去掉系统标题栏和外框
        }
        break;

    case WM_NCHITTEST:
    {
        // 固定尺寸窗口：不再有缩放热区，只保留“标题栏可拖动 + 其余为普通客户区”
        POINT client = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &client);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        if (client.y < kTitleBarHeight && !HitRect(CloseButtonRect(rcClient.right), client.x, client.y)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_NCLBUTTONDBLCLK:
        return 0;   // 双击标题栏不最大化

    case WM_GETMINMAXINFO:
    {
        // 上限和下限都等于当前尺寸：窗口在任何情况下都无法被拉伸/最大化
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = kSettingsWidth;
        mmi->ptMinTrackSize.y = kSettingsHeight;
        mmi->ptMaxTrackSize.x = kSettingsWidth;
        mmi->ptMaxTrackSize.y = kSettingsHeight;
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   // 背景在 WM_PAINT 里一次性画完，避免闪烁

    case WM_PAINT:
        PaintSettingsWindow(hwnd);
        return 0;

    case WM_MOUSEMOVE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const bool hc = HitRect(CloseButtonRect(rc.right), x, y);
        const bool hs = HitRect(SaveButtonRect(rc.right, rc.bottom), x, y);
        const bool hx = HitRect(CancelButtonRect(rc.right, rc.bottom), x, y);
        if (hc != g_hoverClose || hs != g_hoverSave || hx != g_hoverCancel) {
            g_hoverClose = hc;
            g_hoverSave = hs;
            g_hoverCancel = hx;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (!trackingMouse) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            if (TrackMouseEvent(&tme)) {
                trackingMouse = true;
            }
        }
        break;
    }

    case WM_MOUSELEAVE:
        trackingMouse = false;
        if (g_hoverClose || g_hoverSave || g_hoverCancel) {
            g_hoverClose = g_hoverSave = g_hoverCancel = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_LBUTTONUP:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (HitRect(CloseButtonRect(rc.right), x, y)) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (HitRect(SaveButtonRect(rc.right, rc.bottom), x, y)) {
            SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_SAVE_BUTTON, BN_CLICKED), 0);
            return 0;
        }
        if (HitRect(CancelButtonRect(rc.right, rc.bottom), x, y)) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    case WM_CREATE:
    {
        // 打开设置窗口时，先取消所有热键监听
        UnregisterHotKeys(g_hMainWnd);
        trackingMouse = false;
        g_hoverClose = g_hoverSave = g_hoverCancel = false;

        // 统一字体：Microsoft YaHei UI 12px（≈9pt），与 GDI+ 绘制的标题/按钮保持一致。
        // 不显式指定时标准控件会退回 16px 的 SYSTEM_FONT，中文标签会被撑大并裁切。
        static HFONT hUiFont = NULL;
        if (!hUiFont) {
            LOGFONTW lf = { 0 };
            lf.lfHeight = -12;
            lf.lfWeight = FW_NORMAL;
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
            hUiFont = CreateFontIndirectW(&lf);
            if (!hUiFont) {
                hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }
        }

        // 权限判断只做一次：UI 布局（延迟下拉框是否可用）和自启逻辑都要用
        BOOL isAdmin = IsUserAnAdmin();

        // 列宽按该字体实测留余量：
        // “增加透明度:”63px、“当前权限：普通用户”108px、“CONTROL+ALT+SHIFT+PAGEDOWN”约 192px
        const int labelX = 20;        // 分区标题、热键标签
        const int labelWidth = 110;   // 中文标签宽度
        const int fieldX = 135;       // 热键显示框 / 复选框的 X 坐标
        const int fieldWidth = 205;   // 热键显示框：完整名称也不会被截断
        const int fieldHeight = 24;
        const int buttonX = 350;      // “设置”按钮
        const int buttonWidth = 60;

        int yPos = 20 + kTitleBarHeight;   // 顶部 36px 留给自绘标题栏
        wchar_t keyText[128];

        // ============ 第一块：窗口居中 ============
        CreateWindow(L"STATIC", L"窗口居中:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, 22,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hCenterEnable = CreateWindow(L"BUTTON", L"启用",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            fieldX, yPos, 60, 22,
            hwnd, (HMENU)IDC_CENTER_ENABLE, GetModuleHandle(NULL), NULL);
        SendMessage(hCenterEnable, BM_SETCHECK, g_enableCenter ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 28;

        CreateWindow(L"STATIC", L"热键:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, fieldHeight,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hCenterDisplay = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            fieldX, yPos, fieldWidth, fieldHeight,
            hwnd, (HMENU)IDC_CENTER_DISPLAY, GetModuleHandle(NULL), NULL);
        // 设置当前热键显示
        swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_centerModifiers), GetKeyName(g_centerKey));
        SetWindowText(hCenterDisplay, keyText);

        hCenterButton = CreateWindow(L"BUTTON", L"设置",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            buttonX, yPos, buttonWidth, fieldHeight,
            hwnd, (HMENU)IDC_CENTER_BUTTON, GetModuleHandle(NULL), NULL);

        yPos += 44;

        // ============ 第二块：窗口置底 ============
        CreateWindow(L"STATIC", L"窗口置底:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, 22,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hSendToBottomEnable = CreateWindow(L"BUTTON", L"启用",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            fieldX, yPos, 60, 22,
            hwnd, (HMENU)IDC_SEND_TO_BOTTOM_ENABLE, GetModuleHandle(NULL), NULL);
        SendMessage(hSendToBottomEnable, BM_SETCHECK, g_enableSendToBottom ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 28;

        CreateWindow(L"STATIC", L"热键:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, fieldHeight,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hSendToBottomDisplay = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            fieldX, yPos, fieldWidth, fieldHeight,
            hwnd, (HMENU)IDC_SEND_TO_BOTTOM_DISPLAY, GetModuleHandle(NULL), NULL);
        // 设置当前热键显示
        swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_sendToBottomModifiers), GetKeyName(g_sendToBottomKey));
        SetWindowText(hSendToBottomDisplay, keyText);

        hSendToBottomButton = CreateWindow(L"BUTTON", L"设置",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            buttonX, yPos, buttonWidth, fieldHeight,
            hwnd, (HMENU)IDC_SEND_TO_BOTTOM_BUTTON, GetModuleHandle(NULL), NULL);

        yPos += 44;

        // ============ 第三块：窗口抖动 ============
        CreateWindow(L"STATIC", L"窗口抖动:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, 22,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hShakeEnable = CreateWindow(L"BUTTON", L"启用",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            fieldX, yPos, 60, 22,
            hwnd, (HMENU)IDC_SHAKE_ENABLE, GetModuleHandle(NULL), NULL);
        SendMessage(hShakeEnable, BM_SETCHECK, g_enableShake ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 28;

        CreateWindow(L"STATIC", L"热键:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, fieldHeight,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hShakeDisplay = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            fieldX, yPos, fieldWidth, fieldHeight,
            hwnd, (HMENU)IDC_SHAKE_DISPLAY, GetModuleHandle(NULL), NULL);
        // 设置当前热键显示
        swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_shakeModifiers), GetKeyName(g_shakeKey));
        SetWindowText(hShakeDisplay, keyText);

        hShakeButton = CreateWindow(L"BUTTON", L"设置",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            buttonX, yPos, buttonWidth, fieldHeight,
            hwnd, (HMENU)IDC_SHAKE_BUTTON, GetModuleHandle(NULL), NULL);

        yPos += 44;

        // ============ 第四块：透明度控制 ============
        CreateWindow(L"STATIC", L"透明度控制:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, 22,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hTransparencyEnable = CreateWindow(L"BUTTON", L"启用",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            fieldX, yPos, 60, 22,
            hwnd, (HMENU)IDC_TRANSPARENCY_ENABLE, GetModuleHandle(NULL), NULL);
        // 该复选框控制“增加/减少透明度”这对热键；重置透明度是独立的安全网，默认常开
        SendMessage(hTransparencyEnable, BM_SETCHECK, (g_enableTransparencyUp || g_enableTransparencyDown) ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 28;

        // 透明度增加热键
        CreateWindow(L"STATIC", L"增加透明度:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, fieldHeight,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hTransparencyUpDisplay = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            fieldX, yPos, fieldWidth, fieldHeight,
            hwnd, (HMENU)IDC_TRANSPARENCY_UP_DISPLAY, GetModuleHandle(NULL), NULL);
        // 设置当前热键显示
        swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_transparencyUpModifiers), GetKeyName(g_transparencyUpKey));
        SetWindowText(hTransparencyUpDisplay, keyText);

        hTransparencyUpButton = CreateWindow(L"BUTTON", L"设置",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            buttonX, yPos, buttonWidth, fieldHeight,
            hwnd, (HMENU)IDC_TRANSPARENCY_UP_BUTTON, GetModuleHandle(NULL), NULL);

        yPos += 28;

        // 透明度减少热键
        CreateWindow(L"STATIC", L"减少透明度:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, fieldHeight,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hTransparencyDownDisplay = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            fieldX, yPos, fieldWidth, fieldHeight,
            hwnd, (HMENU)IDC_TRANSPARENCY_DOWN_DISPLAY, GetModuleHandle(NULL), NULL);
        // 设置当前热键显示
        swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_transparencyDownModifiers), GetKeyName(g_transparencyDownKey));
        SetWindowText(hTransparencyDownDisplay, keyText);

        hTransparencyDownButton = CreateWindow(L"BUTTON", L"设置",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            buttonX, yPos, buttonWidth, fieldHeight,
            hwnd, (HMENU)IDC_TRANSPARENCY_DOWN_BUTTON, GetModuleHandle(NULL), NULL);

        yPos += 26;

        // 重置透明度热键
        CreateWindow(L"STATIC", L"重置透明度:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, fieldHeight,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hResetTransparencyDisplay = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            fieldX, yPos, fieldWidth, fieldHeight,
            hwnd, (HMENU)IDC_RESET_TRANSPARENCY_DISPLAY, GetModuleHandle(NULL), NULL);
        // 设置当前热键显示
        swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_resetTransparencyModifiers), GetKeyName(g_resetTransparencyKey));
        SetWindowText(hResetTransparencyDisplay, keyText);

        hResetTransparencyButton = CreateWindow(L"BUTTON", L"设置",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            buttonX, yPos, buttonWidth, fieldHeight,
            hwnd, (HMENU)IDC_RESET_TRANSPARENCY_BUTTON, GetModuleHandle(NULL), NULL);

        yPos += 36;

        // ============ 第五块：透明度步长 ============
        wchar_t stepText[64];
        swprintf_s(stepText, 64, L"透明度步长: %d%%", g_transparencyStep);
        hTransparencyLabel = CreateWindow(L"STATIC", stepText,
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, 220, 22,
            hwnd, (HMENU)IDC_TRANSPARENCY_LABEL, GetModuleHandle(NULL), NULL);

        yPos += 26;

        hTransparencySlider = CreateWindow(TRACKBAR_CLASS, L"",
            WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
            labelX, yPos, 390, 30,
            hwnd, (HMENU)IDC_TRANSPARENCY_SLIDER, GetModuleHandle(NULL), NULL);

        SendMessage(hTransparencySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 50));
        SendMessage(hTransparencySlider, TBM_SETPOS, TRUE, g_transparencyStep);
        SendMessage(hTransparencySlider, TBM_SETTICFREQ, 5, 0);

        yPos += 46;

        // ============ 第六块：启动提示开关 ============
        hShowStartupMsg = CreateWindow(L"BUTTON", L"启动成功后显示提示框",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            labelX, yPos, 240, 22,
            hwnd, (HMENU)IDC_SHOW_STARTUP_MSG, GetModuleHandle(NULL), NULL);
        SendMessage(hShowStartupMsg, BM_SETCHECK, g_showStartupMessage ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 32;

        // ============ 第七块：窗口居中选项 ============
        hCenterIgnoreTaskbar = CreateWindow(L"BUTTON", L"居中时忽略任务栏",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            labelX, yPos, 240, 22,
            hwnd, (HMENU)IDC_CENTER_IGNORE_TASKBAR, GetModuleHandle(NULL), NULL);
        SendMessage(hCenterIgnoreTaskbar, BM_SETCHECK, g_centerIgnoreTaskbar ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 34;

        // ============ 第八块：开机自启 ============
        BOOL autoStartOn = IsAutoStartRegistered();
        hAutoStart = CreateWindow(L"BUTTON", L"开机自动启动（静默）",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            labelX, yPos, 260, 22,
            hwnd, (HMENU)IDC_AUTOSTART, GetModuleHandle(NULL), NULL);
        SendMessage(hAutoStart, BM_SETCHECK, autoStartOn ? BST_CHECKED : BST_UNCHECKED, 0);

        yPos += 26;

        CreateWindow(L"STATIC", L"延迟启动（秒）:",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, labelWidth, 22,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hAutoStartDelay = CreateWindow(L"COMBOBOX", NULL,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            fieldX, yPos, 100, 200,
            hwnd, (HMENU)IDC_AUTOSTART_DELAY, GetModuleHandle(NULL), NULL);
        SendMessage(hAutoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"未选择");
        SendMessage(hAutoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"5");
        SendMessage(hAutoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"10");
        SendMessage(hAutoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"15");
        SendMessage(hAutoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"30");

        {
            const int delays[] = { 0, 5, 10, 15, 30 };
            int sel = 0;
            for (int i = 0; i < 5; ++i) {
                if (delays[i] == g_autoStartDelay) {
                    sel = i;
                    break;
                }
            }
            SendMessage(hAutoStartDelay, CB_SETCURSEL, sel, 0);
        }

        CreateWindow(L"STATIC", isAdmin ? L"（登录后延迟启动）" : L"（延迟需管理员权限）",
            WS_VISIBLE | WS_CHILD,
            fieldX + 110, yPos, 200, 22,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        // 普通用户写注册表 Run 无法设置延迟，直接置灰下拉框
        UpdateAutoStartEnabledState(hAutoStart, hAutoStartDelay);

        yPos += 34;

        // ============ 第九块：当前权限状态 ============
        hAdminStatus = CreateWindow(L"STATIC", L"",
            WS_VISIBLE | WS_CHILD,
            labelX, yPos, 200, fieldHeight,
            hwnd, (HMENU)IDC_ADMIN_STATUS, GetModuleHandle(NULL), NULL);

        hElevateButton = CreateWindow(L"BUTTON", L"去提权",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            labelX + 210, yPos, 60, fieldHeight,
            hwnd, (HMENU)IDC_ELEVATE, GetModuleHandle(NULL), NULL);

        SetWindowText(hAdminStatus, isAdmin ? L"当前权限：管理员" : L"当前权限：普通用户");

        // “权限说明”问号图标：LoadIcon 取系统自带图标，再复制成 16×16，
        // 直接用 32×32 的系统图标会在 24px 高的行里被裁切
        HICON hSystemQuestion = LoadIcon(NULL, IDI_QUESTION);
        hPermissionIcon = (HICON)CopyImage(hSystemQuestion, IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        permissionIconOwned = (hPermissionIcon != NULL);
        if (!hPermissionIcon) {
            hPermissionIcon = hSystemQuestion;   // 复制失败时退回共享图标（共享图标不能销毁）
        }

        // 用 BS_ICON 按钮而不是 SS_ICON 静态框：静态框对鼠标是“透明”的
        // （WM_NCHITTEST 返回 HTTRANSPARENT），鼠标消息会直接落到父窗口，
        // TTF_SUBCLASS 收不到悬停事件；按钮则能正常收到 WM_MOUSEMOVE。
        HWND hPermissionHelp = CreateWindow(L"BUTTON", L"",
            WS_VISIBLE | WS_CHILD | BS_ICON | BS_CENTER | BS_VCENTER,
            labelX + 276, yPos + 1, 22, 22,
            hwnd, (HMENU)IDC_PERMISSION_HELP, GetModuleHandle(NULL), NULL);
        SendMessage(hPermissionHelp, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPermissionIcon);

        // 原生 Tooltip：TTF_SUBCLASS 让 tooltip 自己接管图标的鼠标悬停跟踪
        hPermissionTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hwnd, NULL, GetModuleHandle(NULL), NULL);
        if (hPermissionTip) {
            TOOLINFO ti = { 0 };
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = hwnd;
            ti.uId = (UINT_PTR)hPermissionHelp;
            ti.lpszText = g_permissionTipText;

            // 注意：没有 v6 清单时进程用的是系统自带的老版 comctl32（5.82），
            // 它只接受旧版 TOOLINFO 大小（56 字节），传 sizeof(TOOLINFO)（72）会返回 FALSE；
            // 带 v6 清单时又必须传完整大小。这里先按完整大小添加，失败再退回旧大小。
            if (!SendMessageW(hPermissionTip, TTM_ADDTOOLW, 0, (LPARAM)&ti)) {
                ti.cbSize = TTTOOLINFOW_V1_SIZE;
                SendMessageW(hPermissionTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            }
            // 长文本按 380px 自动折行，否则会拉成一条超宽横幅
            SendMessageW(hPermissionTip, TTM_SETMAXTIPWIDTH, 0, 380);
            // 悬停 500ms 弹出（默认偏慢）；移开后 100ms 即可再次提示
            SendMessageW(hPermissionTip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELONG(500, 0));
            SendMessageW(hPermissionTip, TTM_SETDELAYTIME, TTDT_RESHOW, MAKELONG(100, 0));
        }

        if (isAdmin) {
            // 已经是管理员，提权按钮和说明图标都没有意义
            ShowWindow(hElevateButton, SW_HIDE);
            ShowWindow(hPermissionHelp, SW_HIDE);
        }

        // 保存/关闭按钮改为在 WM_PAINT 里用 GDI+ 自绘（见底部区域），这里不再创建原生按钮

        // 统一设置字体（放在最后，一次性覆盖所有已创建的控件）
        EnumChildWindows(hwnd, SetChildFontProc, (LPARAM)hUiFont);

        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        // 静态文本透明绘制，并使用与自绘背景一致的画刷，避免出现灰色方块
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)SettingsBgBrush();
    }

    case WM_CTLCOLORBTN:
        // 复选框等按钮的底色也贴合自绘背景
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (LRESULT)SettingsBgBrush();

    case WM_DESTROY:
        // 设置窗口每次打开都会重新创建 Tooltip 和图标，销毁时必须一并释放，避免句柄泄漏
        if (hPermissionTip) {
            DestroyWindow(hPermissionTip);
            hPermissionTip = NULL;
        }
        if (hPermissionIcon && permissionIconOwned) {
            DestroyIcon(hPermissionIcon);
        }
        hPermissionIcon = NULL;
        permissionIconOwned = false;
        break;

    case WM_HSCROLL:
        if ((HWND)lParam == hTransparencySlider) {
            int pos = (int)SendMessage(hTransparencySlider, TBM_GETPOS, 0, 0);
            wchar_t text[50];
            swprintf_s(text, 50, L"透明度步长: %d%%", pos);
            SetWindowText(hTransparencyLabel, text);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TRANSPARENCY_UP_BUTTON:
        {
            if (g_isListeningHotkey && g_currentListeningType == 1) {
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(hTransparencyUpButton, L"设置");
                // 恢复原来的热键显示
                wchar_t keyText[256];
                swprintf(keyText, 256, L"%s+%s", GetModifierName(g_transparencyUpModifiers), GetKeyName(g_transparencyUpKey));
                SetWindowText(hTransparencyUpDisplay, keyText);
                ReleaseCapture();
            }
            else {
                g_isListeningHotkey = TRUE;
                g_currentListeningType = 1;
                g_hCurrentButton = hTransparencyUpButton;
                g_hCurrentDisplay = hTransparencyUpDisplay;
                g_listeningStartTime = GetTickCount();
                SetWindowText(hTransparencyUpButton, L"取消");
                SetWindowText(hTransparencyUpDisplay, L"请按下组合键...");
                SetFocus(hwnd);
                // SetCapture(hwnd);
            }
            break;
        }
        case IDC_TRANSPARENCY_DOWN_BUTTON:
        {
            if (g_isListeningHotkey && g_currentListeningType == 2) {
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(hTransparencyDownButton, L"设置");
                // 恢复原来的热键显示
                wchar_t keyText[256];
                swprintf(keyText, 256, L"%s+%s", GetModifierName(g_transparencyDownModifiers), GetKeyName(g_transparencyDownKey));
                SetWindowText(hTransparencyDownDisplay, keyText);
                ReleaseCapture();
            }
            else {
                g_isListeningHotkey = TRUE;
                g_currentListeningType = 2;
                g_hCurrentButton = hTransparencyDownButton;
                g_hCurrentDisplay = hTransparencyDownDisplay;
                g_listeningStartTime = GetTickCount();
                SetWindowText(hTransparencyDownButton, L"取消");
                SetWindowText(hTransparencyDownDisplay, L"请按下组合键...");
                SetFocus(hwnd);
                // SetCapture(hwnd);
            }
            break;
        }
        case IDC_CENTER_BUTTON:
        {
            if (g_isListeningHotkey && g_currentListeningType == 3) {
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(hCenterButton, L"设置");
                // 恢复原来的热键显示
                wchar_t keyText[256];
                swprintf(keyText, 256, L"%s+%s", GetModifierName(g_centerModifiers), GetKeyName(g_centerKey));
                SetWindowText(hCenterDisplay, keyText);
                ReleaseCapture();
            }
            else {
                g_isListeningHotkey = TRUE;
                g_currentListeningType = 3;
                g_hCurrentButton = hCenterButton;
                g_hCurrentDisplay = hCenterDisplay;
                g_listeningStartTime = GetTickCount();
                SetWindowText(hCenterButton, L"取消");
                SetWindowText(hCenterDisplay, L"请按下组合键...");
                SetFocus(hwnd);
                // SetCapture(hwnd);
            }
            break;
        }
        case IDC_SHAKE_BUTTON:
        {
            if (g_isListeningHotkey && g_currentListeningType == 4) {
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(hShakeButton, L"设置");
                // 恢复原来的热键显示
                wchar_t keyText[256];
                swprintf(keyText, 256, L"%s+%s", GetModifierName(g_shakeModifiers), GetKeyName(g_shakeKey));
                SetWindowText(hShakeDisplay, keyText);
                ReleaseCapture();
            }
            else {
                g_isListeningHotkey = TRUE;
                g_currentListeningType = 4;
                g_hCurrentButton = hShakeButton;
                g_hCurrentDisplay = hShakeDisplay;
                g_listeningStartTime = GetTickCount();
                SetWindowText(hShakeButton, L"取消");
                SetWindowText(hShakeDisplay, L"请按下组合键...");
                SetFocus(hwnd);
                // SetCapture(hwnd);
            }
            break;
        }
        case IDC_RESET_TRANSPARENCY_BUTTON:
        {
            if (g_isListeningHotkey && g_currentListeningType == 5) {
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(hResetTransparencyButton, L"设置");
                // 恢复原来的热键显示
                wchar_t keyText[128];
                swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_resetTransparencyModifiers), GetKeyName(g_resetTransparencyKey));
                SetWindowText(hResetTransparencyDisplay, keyText);
                ReleaseCapture();
            }
            else {
                g_isListeningHotkey = TRUE;
                g_currentListeningType = 5;
                g_hCurrentButton = hResetTransparencyButton;
                g_hCurrentDisplay = hResetTransparencyDisplay;
                g_listeningStartTime = GetTickCount();
                SetWindowText(hResetTransparencyButton, L"取消");
                SetWindowText(hResetTransparencyDisplay, L"请按下组合键...");
                SetFocus(hwnd);
            }
            break;
        }
        case IDC_SEND_TO_BOTTOM_BUTTON:
        {
            if (g_isListeningHotkey && g_currentListeningType == 6) {
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(hSendToBottomButton, L"设置");
                // 恢复原来的热键显示
                wchar_t keyText[128];
                swprintf_s(keyText, 128, L"%s+%s", GetModifierName(g_sendToBottomModifiers), GetKeyName(g_sendToBottomKey));
                SetWindowText(hSendToBottomDisplay, keyText);
                ReleaseCapture();
            }
            else {
                g_isListeningHotkey = TRUE;
                g_currentListeningType = 6;
                g_hCurrentButton = hSendToBottomButton;
                g_hCurrentDisplay = hSendToBottomDisplay;
                g_listeningStartTime = GetTickCount();
                SetWindowText(hSendToBottomButton, L"取消");
                SetWindowText(hSendToBottomDisplay, L"请按下组合键...");
                SetFocus(hwnd);
            }
            break;
        }
        case IDC_TRANSPARENCY_ENABLE:
        {
            BOOL transparencyEnabled = (SendMessage(hTransparencyEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_enableTransparencyUp = transparencyEnabled;
            g_enableTransparencyDown = transparencyEnabled;
            break;
        }
        case IDC_CENTER_ENABLE:
            g_enableCenter = (SendMessage(hCenterEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;
        case IDC_SHAKE_ENABLE:
            g_enableShake = (SendMessage(hShakeEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;
        case IDC_SEND_TO_BOTTOM_ENABLE:
            g_enableSendToBottom = (SendMessage(hSendToBottomEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;
        case IDC_AUTOSTART:
            UpdateAutoStartEnabledState(hAutoStart, hAutoStartDelay);
            break;
        case IDC_SAVE_BUTTON:
        {
            // 获取透明度步长
            g_transparencyStep = (int)SendMessage(hTransparencySlider, TBM_GETPOS, 0, 0);

            // 获取启动提示开关
            g_showStartupMessage = (SendMessage(hShowStartupMsg, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;

            // 获取“居中时忽略任务栏”开关
            g_centerIgnoreTaskbar = (SendMessage(hCenterIgnoreTaskbar, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;

            // 开机自启：先记住勾选状态和下拉框选择
            BOOL wantAutoStart = (SendMessage(hAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_autoStartDelay = ComboAutoStartDelay(hAutoStartDelay);

            // 保存配置
            SaveConfig();

            // 重新注册热键
            UnregisterHotKeys(g_hMainWnd);
            RegisterHotKeys(g_hMainWnd);

            // 应用开机自启（写注册表或建计划任务），并把结果一并提示给用户
            wchar_t detail[512] = L"";
            bool autoStartOk = ApplyAutoStart(wantAutoStart, g_autoStartDelay, detail, 512);

            wchar_t msg[1024];
            if (autoStartOk) {
                swprintf_s(msg, 1024, L"设置已保存！\n\n开机自启：%s", detail);
                MessageBox(hwnd, msg, L"提示", MB_OK | MB_ICONINFORMATION);
            } else {
                swprintf_s(msg, 1024, L"设置已保存，但开机自启配置失败：\n\n%s", detail);
                MessageBox(hwnd, msg, L"警告", MB_OK | MB_ICONWARNING);
            }
            break;
        }
        case IDC_ELEVATE:
            // 以管理员身份重启（当前实例会被关闭）
            RestartAsAdmin();
            break;
        case IDCANCEL:
            // 取消设置时，重新注册所有热键
            RegisterHotKeys(g_hMainWnd);
            DestroyWindow(hwnd);
            g_hSettingsWnd = NULL;
            break;
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_isListeningHotkey) {
            // 检查超时（10秒）
            if (GetTickCount() - g_listeningStartTime > 10000) {
                g_isListeningHotkey = FALSE;
                SetWindowText(g_hCurrentButton, L"设置");
                SetWindowText(g_hCurrentDisplay, L"监听超时，请重试");
                g_currentListeningType = 0;
                ReleaseCapture();
                break;
            }

            // 检测修饰键
            UINT modifiers = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
            if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
            if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) modifiers |= MOD_WIN;

            // 如果没有修饰键，忽略单独按键（除了功能键）
            if (modifiers == 0 && wParam >= 'A' && wParam <= 'Z') {
                break;
            }

            // 验证热键有效性：必须有修饰键或者是功能键
            if (modifiers == 0 && !(wParam >= VK_F1 && wParam <= VK_F24) &&
                wParam != VK_INSERT && wParam != VK_DELETE && wParam != VK_HOME &&
                wParam != VK_END && wParam != VK_PRIOR && wParam != VK_NEXT &&
                wParam != VK_SPACE && wParam != VK_TAB && wParam != VK_RETURN && wParam != VK_ESCAPE) {
                SetWindowText(g_hCurrentDisplay, L"请使用修饰键组合");
                break;
            }

            // 处理功能键、方向键、字母键和数字键
            if ((wParam >= VK_F1 && wParam <= VK_F24) ||
                (wParam >= VK_LEFT && wParam <= VK_DOWN) ||
                (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) ||
                (wParam >= 'A' && wParam <= 'Z') ||
                (wParam >= '0' && wParam <= '9') ||
                wParam == VK_INSERT || wParam == VK_DELETE ||
                wParam == VK_HOME || wParam == VK_END ||
                wParam == VK_PRIOR || wParam == VK_NEXT ||
                wParam == VK_SPACE || wParam == VK_TAB ||
                wParam == VK_RETURN || wParam == VK_ESCAPE) {

                // 根据当前监听类型更新对应的全局变量
                switch (g_currentListeningType) {
                case 1: // 透明度增加
                    g_transparencyUpModifiers = modifiers;
                    g_transparencyUpKey = (UINT)wParam;
                    break;
                case 2: // 透明度减少
                    g_transparencyDownModifiers = modifiers;
                    g_transparencyDownKey = (UINT)wParam;
                    break;
                case 3: // 窗口居中
                    g_centerModifiers = modifiers;
                    g_centerKey = (UINT)wParam;
                    break;
                case 4: // 窗口抖动
                    g_shakeModifiers = modifiers;
                    g_shakeKey = (UINT)wParam;
                    break;
                case 5: // 重置透明度
                    g_resetTransparencyModifiers = modifiers;
                    g_resetTransparencyKey = (UINT)wParam;
                    break;
                case 6: // 窗口置底
                    g_sendToBottomModifiers = modifiers;
                    g_sendToBottomKey = (UINT)wParam;
                    break;
                }

                // 显示设置的热键：修饰键名称统一由 GetModifierName 生成，
                // 与打开设置窗口时的显示一致（完整名称，不用缩写）
                wchar_t hotkeyText[100] = L"";
                wcscpy_s(hotkeyText, 100, GetModifierName(modifiers));
                if (wcslen(hotkeyText) > 0) wcscat_s(hotkeyText, 100, L"+");

                // 添加按键名称
                switch (wParam) {
                case VK_UP: wcscat_s(hotkeyText, 100, L"UP"); break;
                case VK_DOWN: wcscat_s(hotkeyText, 100, L"DOWN"); break;
                case VK_LEFT: wcscat_s(hotkeyText, 100, L"LEFT"); break;
                case VK_RIGHT: wcscat_s(hotkeyText, 100, L"RIGHT"); break;
                case VK_NUMPAD0: wcscat_s(hotkeyText, 100, L"NUM0"); break;
                case VK_NUMPAD1: wcscat_s(hotkeyText, 100, L"NUM1"); break;
                case VK_NUMPAD2: wcscat_s(hotkeyText, 100, L"NUM2"); break;
                case VK_NUMPAD3: wcscat_s(hotkeyText, 100, L"NUM3"); break;
                case VK_NUMPAD4: wcscat_s(hotkeyText, 100, L"NUM4"); break;
                case VK_NUMPAD5: wcscat_s(hotkeyText, 100, L"NUM5"); break;
                case VK_NUMPAD6: wcscat_s(hotkeyText, 100, L"NUM6"); break;
                case VK_NUMPAD7: wcscat_s(hotkeyText, 100, L"NUM7"); break;
                case VK_NUMPAD8: wcscat_s(hotkeyText, 100, L"NUM8"); break;
                case VK_NUMPAD9: wcscat_s(hotkeyText, 100, L"NUM9"); break;
                case VK_INSERT: wcscat_s(hotkeyText, 100, L"INSERT"); break;
                case VK_DELETE: wcscat_s(hotkeyText, 100, L"DELETE"); break;
                case VK_HOME: wcscat_s(hotkeyText, 100, L"HOME"); break;
                case VK_END: wcscat_s(hotkeyText, 100, L"END"); break;
                case VK_PRIOR: wcscat_s(hotkeyText, 100, L"PAGEUP"); break;
                case VK_NEXT: wcscat_s(hotkeyText, 100, L"PAGEDOWN"); break;
                case VK_SPACE: wcscat_s(hotkeyText, 100, L"SPACE"); break;
                case VK_TAB: wcscat_s(hotkeyText, 100, L"TAB"); break;
                case VK_RETURN: wcscat_s(hotkeyText, 100, L"ENTER"); break;
                case VK_ESCAPE: wcscat_s(hotkeyText, 100, L"ESC"); break;
                default:
                    if (wParam >= VK_F1 && wParam <= VK_F24) {
                        wchar_t fKey[8];
                        swprintf(fKey, 8, L"F%d", (int)(wParam - VK_F1 + 1));
                        wcscat_s(hotkeyText, 100, fKey);
                    }
                    else if (wParam >= 'A' && wParam <= 'Z') {
                        wchar_t letter[2] = { (wchar_t)wParam, L'\0' };
                        wcscat_s(hotkeyText, 100, letter);
                    }
                    else if (wParam >= '0' && wParam <= '9') {
                        wchar_t digit[2] = { (wchar_t)wParam, L'\0' };
                        wcscat_s(hotkeyText, 100, digit);
                    }
                    else {
                        wchar_t keyName[32];
                        swprintf(keyName, 32, L"KEY_%d", (int)wParam);
                        wcscat_s(hotkeyText, 100, keyName);
                    }
                    break;
                }

                SetWindowText(g_hCurrentDisplay, hotkeyText);

                // 停止监听
                g_isListeningHotkey = FALSE;
                g_currentListeningType = 0;
                SetWindowText(g_hCurrentButton, L"设置");
                ReleaseCapture();
            }
        }
        break;

    case WM_CLOSE:
        // 关闭设置窗口时，重新注册所有热键
        RegisterHotKeys(g_hMainWnd);
        DestroyWindow(hwnd);
        g_hSettingsWnd = NULL;
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// 创建系统托盘图标
void CreateTrayIcon(HWND hwnd)
{
    ZeroMemory(&g_nid, sizeof(NOTIFYICONDATA));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    // 直接加载图标资源
    g_nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_WINCLEARFOCUS));
    if (!g_nid.hIcon) {
        // 如果无法加载自定义图标，使用默认应用程序图标
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    swprintf_s(g_nid.szTip, sizeof(g_nid.szTip) / sizeof(wchar_t), L"WinClearFocus %s - 窗口透明度控制", APP_VERSION);

    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// 移除系统托盘图标
void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// 显示右键菜单
void ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// 显示设置窗口
void ShowSettingsWindow()
{
    if (g_hSettingsWnd) {
        SetForegroundWindow(g_hSettingsWnd);
        return;
    }

    // 注册设置窗口类
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SettingsWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;      // 背景完全自绘
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_WINCLEARFOCUS));

    RegisterClass(&wc);

    // 计算窗口居中位置
    int windowWidth = kSettingsWidth;
    int windowHeight = kSettingsHeight;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    // 无边框 + 可缩放边缘；标题栏与圆角由我们自绘/DWM 提供
    wchar_t windowTitle[100];
    swprintf_s(windowTitle, 100, L"WinClearFocus %s 设置", APP_VERSION);
    g_hSettingsWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"SettingsWindowClass",
        windowTitle,
        WS_POPUP | WS_CLIPCHILDREN,
        x, y,
        windowWidth, windowHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (g_hSettingsWnd) {
        // Windows 11：DWM 圆角（Win10 上调用失败无副作用）
        DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(g_hSettingsWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

        ShowWindow(g_hSettingsWnd, SW_SHOW);
        UpdateWindow(g_hSettingsWnd);
    }
}

// 注册全局热键
void RegisterHotKeys(HWND hwnd)
{
    // 注销所有现有热键
    UnregisterHotKeys(hwnd);

    // 根据开关状态注册热键
    if (g_enableTransparencyUp) {
        if (!RegisterHotKey(hwnd, ID_HOTKEY_TRANSPARENCY_UP, g_transparencyUpModifiers, g_transparencyUpKey)) {
            MessageBox(hwnd, L"透明度增加热键注册失败，可能与其他程序冲突", L"警告", MB_OK | MB_ICONWARNING);
        }
    }
    if (g_enableTransparencyDown) {
        if (!RegisterHotKey(hwnd, ID_HOTKEY_TRANSPARENCY_DOWN, g_transparencyDownModifiers, g_transparencyDownKey)) {
            MessageBox(hwnd, L"透明度减少热键注册失败，可能与其他程序冲突", L"警告", MB_OK | MB_ICONWARNING);
        }
    }
    if (g_enableCenter) {
        if (!RegisterHotKey(hwnd, ID_HOTKEY_CENTER_WINDOW, g_centerModifiers, g_centerKey)) {
            MessageBox(hwnd, L"窗口居中热键注册失败，可能与其他程序冲突", L"警告", MB_OK | MB_ICONWARNING);
        }
    }
    if (g_enableShake) {
        if (!RegisterHotKey(hwnd, ID_HOTKEY_SHAKE_WINDOW, g_shakeModifiers, g_shakeKey)) {
            MessageBox(hwnd, L"窗口抖动热键注册失败，可能与其他程序冲突", L"警告", MB_OK | MB_ICONWARNING);
        }
    }
    if (g_enableResetTransparency) {
        if (!RegisterHotKey(hwnd, ID_HOTKEY_RESET_TRANSPARENCY, g_resetTransparencyModifiers, g_resetTransparencyKey)) {
            MessageBox(hwnd, L"重置透明度热键注册失败，可能与其他程序冲突", L"警告", MB_OK | MB_ICONWARNING);
        }
    }
    if (g_enableSendToBottom) {
        if (!RegisterHotKey(hwnd, ID_HOTKEY_SEND_TO_BOTTOM, g_sendToBottomModifiers, g_sendToBottomKey)) {
            MessageBox(hwnd, L"窗口置底热键注册失败，可能与其他程序冲突", L"警告", MB_OK | MB_ICONWARNING);
        }
    }
}

// 注销全局热键
void UnregisterHotKeys(HWND hwnd)
{
    UnregisterHotKey(hwnd, ID_HOTKEY_TRANSPARENCY_UP);
    UnregisterHotKey(hwnd, ID_HOTKEY_TRANSPARENCY_DOWN);
    UnregisterHotKey(hwnd, ID_HOTKEY_CENTER_WINDOW);
    UnregisterHotKey(hwnd, ID_HOTKEY_SHAKE_WINDOW);
    UnregisterHotKey(hwnd, ID_HOTKEY_RESET_TRANSPARENCY);
    UnregisterHotKey(hwnd, ID_HOTKEY_SEND_TO_BOTTOM);
}

// 调整窗口透明度
void AdjustWindowTransparency(BOOL increase)
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || hwnd == g_hMainWnd || hwnd == g_hSettingsWnd) {
        return;
    }

    int currentAlpha = GetWindowTransparency(hwnd);
    int newAlpha;

    if (increase) {
        // 增加透明度（减少alpha值）
        newAlpha = currentAlpha - (255 * g_transparencyStep / 100);
        if (newAlpha < 1) newAlpha = 1; // 极限透明度：最低到 1（近乎全透明，可用“重置透明度”热键恢复）
    }
    else {
        // 减少透明度（增加alpha值）
        newAlpha = currentAlpha + (255 * g_transparencyStep / 100);
        if (newAlpha > 255) newAlpha = 255; // 完全不透明
    }

    SetWindowTransparency(hwnd, newAlpha);
}

// 获取最上层窗口
HWND GetTopMostWindow()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        hwnd = GetWindow(GetDesktopWindow(), GW_CHILD);
    }
    return hwnd;
}

// 设置窗口透明度
void SetWindowTransparency(HWND hwnd, int alpha)
{
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (alpha < 255) {
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA);
    }
    else {
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    }
}

// 获取窗口透明度
int GetWindowTransparency(HWND hwnd)
{
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED) {
        BYTE alpha;
        COLORREF colorKey;
        DWORD flags;
        if (GetLayeredWindowAttributes(hwnd, &colorKey, &alpha, &flags)) {
            return (int)alpha;
        }
    }
    return 255; // 默认完全不透明
}

// 窗口居中：以目标窗口所在显示器的“工作区”为基准，而不是主屏 + 全屏尺寸
void CenterWindow()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || hwnd == g_hMainWnd || hwnd == g_hSettingsWnd) {
        return;
    }

    // 最大化窗口已经铺满工作区，再“居中”反而会把它挪出工作区、压到任务栏下面，
    // 这里直接跳过，保持最大化状态不变
    if (IsZoomed(hwnd)) {
        return;
    }

    // 窗口自身尺寸（含边框，保持原尺寸只挪位置）
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // 目标窗口所在的显示器；跨屏或最小化时取距离最近的那台，避免“飞回主屏”
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (!hMonitor || !GetMonitorInfoW(hMonitor, &mi)) {
        return;
    }

    // rcWork 天然扣除了任务栏（以及停靠的 AppBar），rcMonitor 是整块屏幕
    const RECT& workArea = g_centerIgnoreTaskbar ? mi.rcWork : mi.rcMonitor;
    int workWidth = workArea.right - workArea.left;
    int workHeight = workArea.bottom - workArea.top;

    // 相对工作区左上角计算居中位置
    int x = workArea.left + (workWidth - windowWidth) / 2;
    int y = workArea.top + (workHeight - windowHeight) / 2;

    // SWP_NOACTIVATE：只移动不激活，避免抢走用户当前窗口的焦点
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// 窗口抖动
void ShakeWindow()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || hwnd == g_hMainWnd || hwnd == g_hSettingsWnd) {
        return;
    }

    // 获取窗口当前位置
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int originalX = rect.left;
    int originalY = rect.top;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // 抖动效果：快速移动窗口几次
    const int shakeDistance = 5;
    const int shakeTimes = 6;
    const int shakeDelay = 50; // 毫秒

    for (int i = 0; i < shakeTimes; i++) {
        int offsetX = (i % 2 == 0) ? shakeDistance : -shakeDistance;
        int offsetY = (i % 4 < 2) ? shakeDistance : -shakeDistance;

        SetWindowPos(hwnd, NULL, originalX + offsetX, originalY + offsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        Sleep(shakeDelay);
    }

    // 恢复到原始位置
    SetWindowPos(hwnd, NULL, originalX, originalY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// 重置窗口透明度：把 alpha 恢复成 255（同时摘掉 WS_EX_LAYERED）
void ResetWindowTransparency()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || hwnd == g_hMainWnd || hwnd == g_hSettingsWnd) {
        return;
    }

    SetWindowTransparency(hwnd, 255);
}

// 窗口置底：把当前窗口放到 Z 序最底层（GitHub Issue #3）
void SendWindowToBottom()
{
    HWND hwnd = GetTopMostWindow();
    if (!hwnd || hwnd == g_hMainWnd || hwnd == g_hSettingsWnd) {
        return;
    }

    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// 加载配置
void LoadConfig()
{
    g_transparencyStep = GetPrivateProfileInt(L"Settings", L"TransparencyStep", 10, g_configPath);

    // 启动提示开关：默认显示，首次运行生成的配置也会写成 1
    g_showStartupMessage = GetPrivateProfileInt(L"Settings", L"ShowStartupMessage", 1, g_configPath);

    // 开机自启延迟（秒）：只接受 0/5/10/15/30，其它值一律当 0（未选择）
    g_autoStartDelay = GetPrivateProfileInt(L"Settings", L"AutoStartDelay", 0, g_configPath);
    if (g_autoStartDelay != 5 && g_autoStartDelay != 10 &&
        g_autoStartDelay != 15 && g_autoStartDelay != 30) {
        g_autoStartDelay = 0;
    }

    // 居中时是否忽略任务栏：默认忽略（用工作区 rcWork 居中）
    g_centerIgnoreTaskbar = GetPrivateProfileInt(L"Settings", L"CenterIgnoreTaskbar", 1, g_configPath);

    // 加载热键配置
    // 默认值：窗口居中 Alt+Shift+C，窗口抖动 Alt+Shift+Down，
    //        透明度增加 Alt+Shift+Right，透明度减少 Alt+Shift+Left
    const UINT defaultModifiers = MOD_ALT | MOD_SHIFT;

    g_transparencyUpModifiers = GetPrivateProfileInt(L"Hotkeys", L"TransparencyUpModifiers", defaultModifiers, g_configPath);
    g_transparencyUpKey = GetPrivateProfileInt(L"Hotkeys", L"TransparencyUpKey", VK_RIGHT, g_configPath);
    g_transparencyDownModifiers = GetPrivateProfileInt(L"Hotkeys", L"TransparencyDownModifiers", defaultModifiers, g_configPath);
    g_transparencyDownKey = GetPrivateProfileInt(L"Hotkeys", L"TransparencyDownKey", VK_LEFT, g_configPath);
    // 窗口居中：默认 Alt+Shift+C
    g_centerModifiers = GetPrivateProfileInt(L"Hotkeys", L"CenterModifiers", defaultModifiers, g_configPath);
    g_centerKey = GetPrivateProfileInt(L"Hotkeys", L"CenterKey", 'C', g_configPath);
    // 窗口抖动：默认 Alt+Shift+Down
    g_shakeModifiers = GetPrivateProfileInt(L"Hotkeys", L"ShakeModifiers", defaultModifiers, g_configPath);
    g_shakeKey = GetPrivateProfileInt(L"Hotkeys", L"ShakeKey", VK_DOWN, g_configPath);
    // 重置透明度：默认 Alt+Shift+Up
    g_resetTransparencyModifiers = GetPrivateProfileInt(L"Hotkeys", L"ResetTransparencyModifiers", defaultModifiers, g_configPath);
    g_resetTransparencyKey = GetPrivateProfileInt(L"Hotkeys", L"ResetTransparencyKey", VK_UP, g_configPath);
    // 窗口置底：默认 Alt+Shift+Z
    g_sendToBottomModifiers = GetPrivateProfileInt(L"Hotkeys", L"SendToBottomModifiers", defaultModifiers, g_configPath);
    g_sendToBottomKey = GetPrivateProfileInt(L"Hotkeys", L"SendToBottomKey", 'Z', g_configPath);

    // 加载热键开关状态（默认只启用窗口居中）
    g_enableTransparencyUp = GetPrivateProfileInt(L"Switches", L"EnableTransparencyUp", 0, g_configPath);
    g_enableTransparencyDown = GetPrivateProfileInt(L"Switches", L"EnableTransparencyDown", 0, g_configPath);
    g_enableCenter = GetPrivateProfileInt(L"Switches", L"EnableCenter", 1, g_configPath);
    g_enableShake = GetPrivateProfileInt(L"Switches", L"EnableShake", 0, g_configPath);
    g_enableResetTransparency = GetPrivateProfileInt(L"Switches", L"EnableResetTransparency", 1, g_configPath);
    g_enableSendToBottom = GetPrivateProfileInt(L"Switches", L"EnableSendToBottom", 0, g_configPath);

    // 限制透明度步长范围
    if (g_transparencyStep < 1) g_transparencyStep = 1;
    if (g_transparencyStep > 50) g_transparencyStep = 50;

    // 首次运行（配置文件还不存在）：把上面这套默认值落盘，
    // 这样 ShowStartupMessage 等设置从第一次启动起就是持久化的
    if (GetFileAttributes(g_configPath) == INVALID_FILE_ATTRIBUTES) {
        SaveConfig();
    }
}

// 保存配置
void SaveConfig()
{
    wchar_t value[32];

    // 保存透明度步长
    swprintf(value, 32, L"%d", g_transparencyStep);
    WritePrivateProfileString(L"Settings", L"TransparencyStep", value, g_configPath);

    // 保存启动提示开关
    swprintf(value, 32, L"%d", g_showStartupMessage);
    WritePrivateProfileString(L"Settings", L"ShowStartupMessage", value, g_configPath);

    // 保存开机自启延迟
    swprintf(value, 32, L"%d", g_autoStartDelay);
    WritePrivateProfileString(L"Settings", L"AutoStartDelay", value, g_configPath);

    // 保存“居中时忽略任务栏”
    swprintf(value, 32, L"%d", g_centerIgnoreTaskbar);
    WritePrivateProfileString(L"Settings", L"CenterIgnoreTaskbar", value, g_configPath);

    // 保存热键配置
    swprintf(value, 32, L"%d", g_transparencyUpModifiers);
    WritePrivateProfileString(L"Hotkeys", L"TransparencyUpModifiers", value, g_configPath);
    swprintf(value, 32, L"%d", g_transparencyUpKey);
    WritePrivateProfileString(L"Hotkeys", L"TransparencyUpKey", value, g_configPath);
    swprintf(value, 32, L"%d", g_transparencyDownModifiers);
    WritePrivateProfileString(L"Hotkeys", L"TransparencyDownModifiers", value, g_configPath);
    swprintf(value, 32, L"%d", g_transparencyDownKey);
    WritePrivateProfileString(L"Hotkeys", L"TransparencyDownKey", value, g_configPath);
    swprintf(value, 32, L"%d", g_centerModifiers);
    WritePrivateProfileString(L"Hotkeys", L"CenterModifiers", value, g_configPath);
    swprintf(value, 32, L"%d", g_centerKey);
    WritePrivateProfileString(L"Hotkeys", L"CenterKey", value, g_configPath);
    swprintf(value, 32, L"%d", g_shakeModifiers);
    WritePrivateProfileString(L"Hotkeys", L"ShakeModifiers", value, g_configPath);
    swprintf(value, 32, L"%d", g_shakeKey);
    WritePrivateProfileString(L"Hotkeys", L"ShakeKey", value, g_configPath);
    swprintf(value, 32, L"%d", g_resetTransparencyModifiers);
    WritePrivateProfileString(L"Hotkeys", L"ResetTransparencyModifiers", value, g_configPath);
    swprintf(value, 32, L"%d", g_resetTransparencyKey);
    WritePrivateProfileString(L"Hotkeys", L"ResetTransparencyKey", value, g_configPath);
    swprintf(value, 32, L"%d", g_sendToBottomModifiers);
    WritePrivateProfileString(L"Hotkeys", L"SendToBottomModifiers", value, g_configPath);
    swprintf(value, 32, L"%d", g_sendToBottomKey);
    WritePrivateProfileString(L"Hotkeys", L"SendToBottomKey", value, g_configPath);

    // 保存热键开关状态
    swprintf(value, 32, L"%d", g_enableTransparencyUp ? 1 : 0);
    WritePrivateProfileString(L"Switches", L"EnableTransparencyUp", value, g_configPath);
    swprintf(value, 32, L"%d", g_enableTransparencyDown ? 1 : 0);
    WritePrivateProfileString(L"Switches", L"EnableTransparencyDown", value, g_configPath);
    swprintf(value, 32, L"%d", g_enableCenter ? 1 : 0);
    WritePrivateProfileString(L"Switches", L"EnableCenter", value, g_configPath);
    swprintf(value, 32, L"%d", g_enableShake ? 1 : 0);
    WritePrivateProfileString(L"Switches", L"EnableShake", value, g_configPath);
    swprintf(value, 32, L"%d", g_enableResetTransparency ? 1 : 0);
    WritePrivateProfileString(L"Switches", L"EnableResetTransparency", value, g_configPath);
    swprintf(value, 32, L"%d", g_enableSendToBottom ? 1 : 0);
    WritePrivateProfileString(L"Switches", L"EnableSendToBottom", value, g_configPath);
}

// 获取修饰键名称（使用完整全称，避免缩写导致显示不完整或歧义）
wchar_t* GetModifierName(UINT modifiers)
{
    static wchar_t modifierText[64];
    modifierText[0] = L'\0';

    if (modifiers & MOD_CONTROL) {
        wcscat_s(modifierText, 64, L"CONTROL");
    }
    if (modifiers & MOD_ALT) {
        if (wcslen(modifierText) > 0) wcscat_s(modifierText, 64, L"+");
        wcscat_s(modifierText, 64, L"ALT");
    }
    if (modifiers & MOD_SHIFT) {
        if (wcslen(modifierText) > 0) wcscat_s(modifierText, 64, L"+");
        wcscat_s(modifierText, 64, L"SHIFT");
    }
    if (modifiers & MOD_WIN) {
        if (wcslen(modifierText) > 0) wcscat_s(modifierText, 64, L"+");
        wcscat_s(modifierText, 64, L"WIN");
    }

    return modifierText;
}

// 获取按键名称
wchar_t* GetKeyName(UINT vkCode)
{
    static wchar_t keyText[32];

    switch (vkCode) {
        // 功能键
    case VK_F1:
        wcscpy_s(keyText, 32, L"F1");
        return keyText;
    case VK_F2:
        wcscpy_s(keyText, 32, L"F2");
        return keyText;
    case VK_F3:
        wcscpy_s(keyText, 32, L"F3");
        return keyText;
    case VK_F4:
        wcscpy_s(keyText, 32, L"F4");
        return keyText;
    case VK_F5:
        wcscpy_s(keyText, 32, L"F5");
        return keyText;
    case VK_F6:
        wcscpy_s(keyText, 32, L"F6");
        return keyText;
    case VK_F7:
        wcscpy_s(keyText, 32, L"F7");
        return keyText;
    case VK_F8:
        wcscpy_s(keyText, 32, L"F8");
        return keyText;
    case VK_F9:
        wcscpy_s(keyText, 32, L"F9");
        return keyText;
    case VK_F10:
        wcscpy_s(keyText, 32, L"F10");
        return keyText;
    case VK_F11:
        wcscpy_s(keyText, 32, L"F11");
        return keyText;
    case VK_F12:
        wcscpy_s(keyText, 32, L"F12");
        return keyText;

        // 方向键
    case VK_LEFT:
        wcscpy_s(keyText, 32, L"LEFT");
        return keyText;
    case VK_RIGHT:
        wcscpy_s(keyText, 32, L"RIGHT");
        return keyText;
    case VK_UP:
        wcscpy_s(keyText, 32, L"UP");
        return keyText;
    case VK_DOWN:
        wcscpy_s(keyText, 32, L"DOWN");
        return keyText;

        // 数字键盘
    case VK_NUMPAD0:
        wcscpy_s(keyText, 32, L"NUM0");
        return keyText;
    case VK_NUMPAD1:
        wcscpy_s(keyText, 32, L"NUM1");
        return keyText;
    case VK_NUMPAD2:
        wcscpy_s(keyText, 32, L"NUM2");
        return keyText;
    case VK_NUMPAD3:
        wcscpy_s(keyText, 32, L"NUM3");
        return keyText;
    case VK_NUMPAD4:
        wcscpy_s(keyText, 32, L"NUM4");
        return keyText;
    case VK_NUMPAD5:
        wcscpy_s(keyText, 32, L"NUM5");
        return keyText;
    case VK_NUMPAD6:
        wcscpy_s(keyText, 32, L"NUM6");
        return keyText;
    case VK_NUMPAD7:
        wcscpy_s(keyText, 32, L"NUM7");
        return keyText;
    case VK_NUMPAD8:
        wcscpy_s(keyText, 32, L"NUM8");
        return keyText;
    case VK_NUMPAD9:
        wcscpy_s(keyText, 32, L"NUM9");
        return keyText;

        // 特殊键
    case VK_INSERT:
        wcscpy_s(keyText, 32, L"INSERT");
        return keyText;
    case VK_DELETE:
        wcscpy_s(keyText, 32, L"DELETE");
        return keyText;
    case VK_HOME:
        wcscpy_s(keyText, 32, L"HOME");
        return keyText;
    case VK_END:
        wcscpy_s(keyText, 32, L"END");
        return keyText;
    case VK_PRIOR:
        wcscpy_s(keyText, 32, L"PAGEUP");
        return keyText;
    case VK_NEXT:
        wcscpy_s(keyText, 32, L"PAGEDOWN");
        return keyText;
    case VK_SPACE:
        wcscpy_s(keyText, 32, L"SPACE");
        return keyText;
    case VK_TAB:
        wcscpy_s(keyText, 32, L"TAB");
        return keyText;
    case VK_RETURN:
        wcscpy_s(keyText, 32, L"ENTER");
        return keyText;
    case VK_ESCAPE:
        wcscpy_s(keyText, 32, L"ESC");
        return keyText;

        // 字母和数字键
    default:
        if (vkCode >= 'A' && vkCode <= 'Z') {
            swprintf_s(keyText, 32, L"%c", (wchar_t)vkCode);
            return keyText;
        }
        if (vkCode >= '0' && vkCode <= '9') {
            swprintf_s(keyText, 32, L"%c", (wchar_t)vkCode);
            return keyText;
        }
        swprintf_s(keyText, 32, L"KEY%d", vkCode);
        return keyText;
    }
}