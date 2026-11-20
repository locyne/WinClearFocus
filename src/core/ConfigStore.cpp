#include "core/ConfigStore.h"

#include "core/Settings.h"
#include "util/Win32Utils.h"

#include <stdio.h>
#include <string.h>

namespace wcf {
namespace {

void WriteInt(const wchar_t* section, const wchar_t* key, int value)
{
    wchar_t buffer[32];
    swprintf_s(buffer, 32, L"%d", value);
    WritePrivateProfileStringW(section, key, buffer, ConfigPath());
}

void WriteHotkey(const wchar_t* prefix, const HotkeyBinding& binding)
{
    wchar_t key[64];

    swprintf_s(key, 64, L"%sModifiers", prefix);
    WriteInt(L"Hotkeys", key, (int)binding.modifiers);

    swprintf_s(key, 64, L"%sKey", prefix);
    WriteInt(L"Hotkeys", key, (int)binding.key);
}

void ReadHotkey(const wchar_t* prefix, HotkeyAction action,
                UINT defaultModifiers, UINT defaultKey)
{
    wchar_t key[64];

    swprintf_s(key, 64, L"%sModifiers", prefix);
    const int modifiers = GetPrivateProfileIntW(L"Hotkeys", key, (int)defaultModifiers, ConfigPath());

    swprintf_s(key, 64, L"%sKey", prefix);
    const int vk = GetPrivateProfileIntW(L"Hotkeys", key, (int)defaultKey, ConfigPath());

    HotkeyBinding& binding = Hotkey(action);
    binding.modifiers = (UINT)modifiers;
    binding.key = (UINT)vk;
}

} // namespace

const wchar_t* ConfigPath()
{
    static wchar_t path[MAX_PATH] = { 0 };
    if (path[0]) {
        return path;
    }

    wchar_t exePath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        // 极端情况下退回当前目录
        wcscpy_s(path, MAX_PATH, L"config.ini");
        return path;
    }

    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) {
        *(slash + 1) = L'\0';   // 保留目录部分（含末尾反斜杠）
    } else {
        exePath[0] = L'\0';
    }

    swprintf_s(path, MAX_PATH, L"%sconfig.ini", exePath);
    return path;
}

void LoadConfig()
{
    AppSettings& s = Settings();

    s.transparencyStep = GetPrivateProfileIntW(L"Settings", L"TransparencyStep", 10, ConfigPath());

    // 启动提示开关：默认显示，首次运行生成的配置也会写成 1
    s.showStartupMessage = GetPrivateProfileIntW(L"Settings", L"ShowStartupMessage", 1, ConfigPath());

    // 开机自启延迟（秒）：只接受 0/5/10/15/30，其它值一律当 0（未选择）
    s.autoStartDelay = GetPrivateProfileIntW(L"Settings", L"AutoStartDelay", 0, ConfigPath());
    if (s.autoStartDelay != 5 && s.autoStartDelay != 10 &&
        s.autoStartDelay != 15 && s.autoStartDelay != 30) {
        s.autoStartDelay = 0;
    }

    // 居中时是否忽略任务栏：默认忽略（用工作区 rcWork 居中）
    s.centerIgnoreTaskbar = GetPrivateProfileIntW(L"Settings", L"CenterIgnoreTaskbar", 1, ConfigPath());

    // 热键默认值：全部 Alt+Shift，只有窗口居中默认启用
    const UINT defaultModifiers = MOD_ALT | MOD_SHIFT;
    ReadHotkey(L"TransparencyUp",   HotkeyAction::TransparencyUp,   defaultModifiers, VK_RIGHT);
    ReadHotkey(L"TransparencyDown", HotkeyAction::TransparencyDown, defaultModifiers, VK_LEFT);
    ReadHotkey(L"Center",           HotkeyAction::CenterWindow,     defaultModifiers, 'C');
    ReadHotkey(L"Shake",            HotkeyAction::ShakeWindow,      defaultModifiers, VK_DOWN);
    ReadHotkey(L"ResetTransparency", HotkeyAction::ResetTransparency, defaultModifiers, VK_UP);
    ReadHotkey(L"SendToBottom",     HotkeyAction::SendToBottom,     defaultModifiers, 'Z');
    ReadHotkey(L"Regulate",         HotkeyAction::Regulate,         defaultModifiers, 'R');

    // 加载热键开关状态（默认只启用窗口居中）
    Hotkey(HotkeyAction::TransparencyUp).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableTransparencyUp", 0, ConfigPath());
    Hotkey(HotkeyAction::TransparencyDown).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableTransparencyDown", 0, ConfigPath());
    Hotkey(HotkeyAction::CenterWindow).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableCenter", 1, ConfigPath());
    Hotkey(HotkeyAction::ShakeWindow).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableShake", 0, ConfigPath());
    Hotkey(HotkeyAction::ResetTransparency).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableResetTransparency", 0, ConfigPath());
    Hotkey(HotkeyAction::SendToBottom).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableSendToBottom", 0, ConfigPath());
    Hotkey(HotkeyAction::Regulate).enabled =
        GetPrivateProfileIntW(L"Switches", L"EnableRegulate", 0, ConfigPath());

    // 窗口大小调整参数（统一做范围钳制）
    s.regulateMode = GetPrivateProfileIntW(L"Settings", L"RegulateMode", 0, ConfigPath());
    if (s.regulateMode != 0 && s.regulateMode != 1) {
        s.regulateMode = 0;
    }
    s.regulateScale = ClampInt(
        GetPrivateProfileIntW(L"Settings", L"RegulateScale", 70, ConfigPath()), 30, 90);
    s.regulateIgnoreTaskbar =
        GetPrivateProfileIntW(L"Settings", L"RegulateIgnoreTaskbar", 1, ConfigPath());
    s.regulateWidth = ClampInt(
        GetPrivateProfileIntW(L"Settings", L"RegulateWidth", 1280, ConfigPath()), 100, 8000);
    s.regulateHeight = ClampInt(
        GetPrivateProfileIntW(L"Settings", L"RegulateHeight", 720, ConfigPath()), 100, 8000);
    s.regulateCenter = GetPrivateProfileIntW(L"Settings", L"RegulateCenter", 1, ConfigPath());

    // 限制透明度步长范围
    s.transparencyStep = ClampInt(s.transparencyStep, 1, 50);

    // 首次运行（配置文件还不存在）：把默认值落盘，
    // 这样 ShowStartupMessage 等设置从第一次启动起就是持久化的
    if (GetFileAttributesW(ConfigPath()) == INVALID_FILE_ATTRIBUTES) {
        SaveConfig();
    }
}

void SaveConfig()
{
    const AppSettings& s = Settings();

    WriteInt(L"Settings", L"TransparencyStep", s.transparencyStep);
    WriteInt(L"Settings", L"ShowStartupMessage", s.showStartupMessage);
    WriteInt(L"Settings", L"AutoStartDelay", s.autoStartDelay);
    WriteInt(L"Settings", L"CenterIgnoreTaskbar", s.centerIgnoreTaskbar);

    WriteInt(L"Settings", L"RegulateMode", s.regulateMode);
    WriteInt(L"Settings", L"RegulateScale", s.regulateScale);
    WriteInt(L"Settings", L"RegulateIgnoreTaskbar", s.regulateIgnoreTaskbar);
    WriteInt(L"Settings", L"RegulateWidth", s.regulateWidth);
    WriteInt(L"Settings", L"RegulateHeight", s.regulateHeight);
    WriteInt(L"Settings", L"RegulateCenter", s.regulateCenter);

    WriteHotkey(L"TransparencyUp",   Hotkey(HotkeyAction::TransparencyUp));
    WriteHotkey(L"TransparencyDown", Hotkey(HotkeyAction::TransparencyDown));
    WriteHotkey(L"Center",           Hotkey(HotkeyAction::CenterWindow));
    WriteHotkey(L"Shake",            Hotkey(HotkeyAction::ShakeWindow));
    WriteHotkey(L"ResetTransparency", Hotkey(HotkeyAction::ResetTransparency));
    WriteHotkey(L"SendToBottom",     Hotkey(HotkeyAction::SendToBottom));
    WriteHotkey(L"Regulate",         Hotkey(HotkeyAction::Regulate));

    WriteInt(L"Switches", L"EnableTransparencyUp", Hotkey(HotkeyAction::TransparencyUp).enabled ? 1 : 0);
    WriteInt(L"Switches", L"EnableTransparencyDown", Hotkey(HotkeyAction::TransparencyDown).enabled ? 1 : 0);
    WriteInt(L"Switches", L"EnableCenter", Hotkey(HotkeyAction::CenterWindow).enabled ? 1 : 0);
    WriteInt(L"Switches", L"EnableShake", Hotkey(HotkeyAction::ShakeWindow).enabled ? 1 : 0);
    WriteInt(L"Switches", L"EnableResetTransparency", Hotkey(HotkeyAction::ResetTransparency).enabled ? 1 : 0);
    WriteInt(L"Switches", L"EnableSendToBottom", Hotkey(HotkeyAction::SendToBottom).enabled ? 1 : 0);
    WriteInt(L"Switches", L"EnableRegulate", Hotkey(HotkeyAction::Regulate).enabled ? 1 : 0);
}

} // namespace wcf
