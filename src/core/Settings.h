#pragma once

#include <windows.h>

namespace wcf {

// 每个可配置功能对应一个热键动作。枚举顺序同时决定热键 ID 的顺序，
// 必须与 app/Constants.h 中的 ID_HOTKEY_* 保持一致。
enum class HotkeyAction : int {
    TransparencyUp = 0,
    TransparencyDown,
    CenterWindow,
    ShakeWindow,
    ResetTransparency,
    SendToBottom,
    Regulate,
    Count
};

struct HotkeyBinding {
    UINT modifiers;   // MOD_ALT / MOD_CONTROL / MOD_SHIFT / MOD_WIN 的组合
    UINT key;         // Windows 虚拟键码
    BOOL enabled;     // 是否注册该热键
};

// 程序运行期的全部设置。由 ConfigStore 负责与 config.ini 之间同步。
struct AppSettings {
    int transparencyStep;
    int showStartupMessage;
    int autoStartDelay;
    int centerIgnoreTaskbar;
    int regulateMode;          // 0 = 百分比缩放，1 = 固定宽高
    int regulateScale;
    int regulateIgnoreTaskbar;
    int regulateWidth;
    int regulateHeight;
    int regulateCenter;
    HotkeyBinding hotkeys[static_cast<int>(HotkeyAction::Count)];
};

// 全局唯一设置实例（替代原先散落的 30 多个全局变量）。
AppSettings& Settings();

// 按动作取热键绑定的便捷函数。
HotkeyBinding& Hotkey(HotkeyAction action);

} // namespace wcf
