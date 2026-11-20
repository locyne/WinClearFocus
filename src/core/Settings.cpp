#include "core/Settings.h"

namespace wcf {
namespace {

AppSettings MakeDefaultSettings()
{
    AppSettings s = {};

    s.transparencyStep = 10;
    s.showStartupMessage = 1;
    s.autoStartDelay = 0;
    s.centerIgnoreTaskbar = 1;

    s.regulateMode = 0;
    s.regulateScale = 70;
    s.regulateIgnoreTaskbar = 1;
    s.regulateWidth = 1280;
    s.regulateHeight = 720;
    s.regulateCenter = 1;

    const UINT defaultModifiers = MOD_ALT | MOD_SHIFT;

    struct DefaultHotkey {
        HotkeyAction action;
        UINT key;
        BOOL enabled;
    };
    const DefaultHotkey defaults[] = {
        { HotkeyAction::TransparencyUp,   VK_RIGHT, FALSE },
        { HotkeyAction::TransparencyDown, VK_LEFT,  FALSE },
        { HotkeyAction::CenterWindow,     'C',      TRUE  },
        { HotkeyAction::ShakeWindow,      VK_DOWN,  FALSE },
        { HotkeyAction::ResetTransparency, VK_UP,   FALSE },
        { HotkeyAction::SendToBottom,     'Z',      FALSE },
        { HotkeyAction::Regulate,         'R',      FALSE },
    };

    for (const DefaultHotkey& d : defaults) {
        HotkeyBinding& binding = s.hotkeys[static_cast<int>(d.action)];
        binding.modifiers = defaultModifiers;
        binding.key = d.key;
        binding.enabled = d.enabled;
    }

    return s;
}

} // namespace

AppSettings& Settings()
{
    static AppSettings settings = MakeDefaultSettings();
    return settings;
}

HotkeyBinding& Hotkey(HotkeyAction action)
{
    return Settings().hotkeys[static_cast<int>(action)];
}

} // namespace wcf
