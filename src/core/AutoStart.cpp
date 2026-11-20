#include "core/AutoStart.h"

#include "app/Application.h"

#include <stdio.h>
#include <string.h>

namespace wcf {
namespace {

#define AUTOSTART_RUN_KEY    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTOSTART_RUN_VALUE  L"WinClearFocus"
#define AUTOSTART_TASK_PATH  L"\\WinClearFocus\\WinClearFocus"

// 取系统目录下 schtasks.exe 的完整路径：不要按 PATH 搜索，
// 否则程序目录里放一个同名 exe 就能劫持我们（尤其当程序以管理员运行时）
const wchar_t* SchtasksPath()
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
bool RunProcess(const wchar_t* cmdLine, DWORD* exitCode)
{
    wchar_t buffer[2048];
    wcscpy_s(buffer, 2048, cmdLine);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              OPEN_EXISTING, 0, NULL);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = hNul;
    si.hStdError = hNul;

    PROCESS_INFORMATION pi = { 0 };
    BOOL started = CreateProcessW(NULL, buffer, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
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
bool RunValueExists()
{
    wchar_t value[MAX_PATH + 32] = { 0 };
    DWORD size = sizeof(value);
    LONG r = RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, AUTOSTART_RUN_VALUE,
                          RRF_RT_REG_SZ, NULL, value, &size);
    return r == ERROR_SUCCESS;
}

// 写入/删除注册表启动项，值形如 "C:\path\WinClearFocus.exe" -silent
bool SetRunValue(bool enable)
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
bool TaskExists()
{
    wchar_t cmd[512];
    swprintf_s(cmd, 512, L"%s /query /tn \"%s\"", SchtasksPath(), AUTOSTART_TASK_PATH);
    DWORD code = 1;
    RunProcess(cmd, &code);
    return code == 0;
}

// 创建计划任务：登录时（可延迟）以最高权限启动 "<exe>" -silent
bool CreateScheduledTask(int delaySeconds)
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
bool DeleteScheduledTask()
{
    wchar_t cmd[512];
    swprintf_s(cmd, 512, L"%s /delete /tn \"%s\" /f", SchtasksPath(), AUTOSTART_TASK_PATH);
    DWORD code = 1;
    RunProcess(cmd, &code);
    return code == 0;
}

} // namespace

bool IsAutoStartRegistered()
{
    if (RunValueExists()) {
        return true;
    }
    return TaskExists();
}

bool ApplyAutoStart(BOOL enable, int delaySeconds, wchar_t* detail, size_t detailLen)
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
            swprintf_s(detail, detailLen,
                       L"已移除注册表启动项，但计划任务 %s 需要管理员权限才能删除。",
                       AUTOSTART_TASK_PATH);
            return false;
        }
        swprintf_s(detail, detailLen, L"已关闭（注册表启动项和计划任务均已清理）。");
        return true;
    }

    if (!IsRunningAsAdmin()) {
        // 普通用户：只能写 HKCU Run，无法设置延迟
        if (!SetRunValue(true)) {
            swprintf_s(detail, detailLen, L"写入注册表 HKCU\\...\\Run 失败（错误码 %lu）。",
                       GetLastError());
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
        swprintf_s(detail, detailLen,
                   L"已启用（注册表 HKCU\\...\\Run，普通用户权限不支持延迟启动）。");
        return true;
    }

    // 管理员：先无条件删除可能残留的同名任务，再用统一参数重建一个干净的。
    // 任务不存在时 schtasks /delete 会返回非 0，这里刻意忽略返回值（静默清理，不打扰用户）
    DeleteScheduledTask();

    if (!CreateScheduledTask(delaySeconds)) {
        swprintf_s(detail, detailLen,
                   L"创建计划任务 %s 失败。请确认程序以管理员身份运行，且系统允许创建计划任务。",
                   AUTOSTART_TASK_PATH);
        return false;
    }

    // 防止双重启动：计划任务建好后删掉旧的注册表启动项
    SetRunValue(false);

    if (delaySeconds > 0) {
        swprintf_s(detail, detailLen,
                   L"已启用（计划任务 %s，登录后延迟 %d 秒，最高权限）。",
                   AUTOSTART_TASK_PATH, delaySeconds);
    } else {
        swprintf_s(detail, detailLen,
                   L"已启用（计划任务 %s，登录时启动，最高权限）。", AUTOSTART_TASK_PATH);
    }
    return true;
}

} // namespace wcf
