#include "core/HotkeyManager.h"

#include "app/Constants.h"

namespace wcf {
namespace {

struct HotkeyEntry {
    int id;
    const wchar_t* failureMessage;
};

const HotkeyEntry kEntries[] = {
    { ID_HOTKEY_TRANSPARENCY_UP,    L"透明度增加热键注册失败，可能与其他程序冲突" },
    { ID_HOTKEY_TRANSPARENCY_DOWN,  L"透明度减少热键注册失败，可能与其他程序冲突" },
    { ID_HOTKEY_CENTER_WINDOW,      L"窗口居中热键注册失败，可能与其他程序冲突" },
    { ID_HOTKEY_SHAKE_WINDOW,       L"窗口抖动热键注册失败，可能与其他程序冲突" },
    { ID_HOTKEY_RESET_TRANSPARENCY, L"重置透明度热键注册失败，可能与其他程序冲突" },
    { ID_HOTKEY_SEND_TO_BOTTOM,     L"窗口置底热键注册失败，可能与其他程序冲突" },
    { ID_HOTKEY_REGULATE,           L"窗口大小调整热键注册失败，可能与其他程序冲突" },
};

const int kEntryCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));

} // namespace

int HotkeyId(HotkeyAction action)
{
    const int index = static_cast<int>(action);
    if (index < 0 || index >= kEntryCount) {
        return 0;
    }
    return kEntries[index].id;
}

bool HotkeyActionFromId(int id, HotkeyAction* action)
{
    for (int i = 0; i < kEntryCount; ++i) {
        if (kEntries[i].id == id) {
            if (action) {
                *action = static_cast<HotkeyAction>(i);
            }
            return true;
        }
    }
    return false;
}

void RegisterHotkeys(HWND hwnd)
{
    UnregisterHotkeys(hwnd);

    for (int i = 0; i < kEntryCount; ++i) {
        const HotkeyBinding& binding = Settings().hotkeys[i];
        if (!binding.enabled) {
            continue;
        }

        if (!RegisterHotKey(hwnd, kEntries[i].id, binding.modifiers, binding.key)) {
            MessageBoxW(hwnd, kEntries[i].failureMessage, L"警告", MB_OK | MB_ICONWARNING);
        }
    }
}

void UnregisterHotkeys(HWND hwnd)
{
    for (int i = 0; i < kEntryCount; ++i) {
        UnregisterHotKey(hwnd, kEntries[i].id);
    }
}

} // namespace wcf
