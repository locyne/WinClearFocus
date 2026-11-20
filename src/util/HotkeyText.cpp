#include "util/HotkeyText.h"

#include <stdio.h>
#include <string.h>

namespace wcf {

const wchar_t* ModifierName(UINT modifiers)
{
    static wchar_t text[64];
    text[0] = L'\0';

    if (modifiers & MOD_CONTROL) {
        wcscat_s(text, 64, L"CONTROL");
    }
    if (modifiers & MOD_ALT) {
        if (wcslen(text) > 0) wcscat_s(text, 64, L"+");
        wcscat_s(text, 64, L"ALT");
    }
    if (modifiers & MOD_SHIFT) {
        if (wcslen(text) > 0) wcscat_s(text, 64, L"+");
        wcscat_s(text, 64, L"SHIFT");
    }
    if (modifiers & MOD_WIN) {
        if (wcslen(text) > 0) wcscat_s(text, 64, L"+");
        wcscat_s(text, 64, L"WIN");
    }

    return text;
}

const wchar_t* KeyName(UINT vkCode)
{
    static wchar_t text[32];

    // 功能键 F1 ~ F24
    if (vkCode >= VK_F1 && vkCode <= VK_F24) {
        swprintf_s(text, 32, L"F%d", (int)(vkCode - VK_F1 + 1));
        return text;
    }

    switch (vkCode) {
    case VK_LEFT:
        wcscpy_s(text, 32, L"LEFT");
        return text;
    case VK_RIGHT:
        wcscpy_s(text, 32, L"RIGHT");
        return text;
    case VK_UP:
        wcscpy_s(text, 32, L"UP");
        return text;
    case VK_DOWN:
        wcscpy_s(text, 32, L"DOWN");
        return text;

    case VK_NUMPAD0:
        wcscpy_s(text, 32, L"NUM0");
        return text;
    case VK_NUMPAD1:
        wcscpy_s(text, 32, L"NUM1");
        return text;
    case VK_NUMPAD2:
        wcscpy_s(text, 32, L"NUM2");
        return text;
    case VK_NUMPAD3:
        wcscpy_s(text, 32, L"NUM3");
        return text;
    case VK_NUMPAD4:
        wcscpy_s(text, 32, L"NUM4");
        return text;
    case VK_NUMPAD5:
        wcscpy_s(text, 32, L"NUM5");
        return text;
    case VK_NUMPAD6:
        wcscpy_s(text, 32, L"NUM6");
        return text;
    case VK_NUMPAD7:
        wcscpy_s(text, 32, L"NUM7");
        return text;
    case VK_NUMPAD8:
        wcscpy_s(text, 32, L"NUM8");
        return text;
    case VK_NUMPAD9:
        wcscpy_s(text, 32, L"NUM9");
        return text;

    case VK_INSERT:
        wcscpy_s(text, 32, L"INSERT");
        return text;
    case VK_DELETE:
        wcscpy_s(text, 32, L"DELETE");
        return text;
    case VK_HOME:
        wcscpy_s(text, 32, L"HOME");
        return text;
    case VK_END:
        wcscpy_s(text, 32, L"END");
        return text;
    case VK_PRIOR:
        wcscpy_s(text, 32, L"PAGEUP");
        return text;
    case VK_NEXT:
        wcscpy_s(text, 32, L"PAGEDOWN");
        return text;
    case VK_SPACE:
        wcscpy_s(text, 32, L"SPACE");
        return text;
    case VK_TAB:
        wcscpy_s(text, 32, L"TAB");
        return text;
    case VK_RETURN:
        wcscpy_s(text, 32, L"ENTER");
        return text;
    case VK_ESCAPE:
        wcscpy_s(text, 32, L"ESC");
        return text;

    default:
        if (vkCode >= 'A' && vkCode <= 'Z') {
            swprintf_s(text, 32, L"%c", (wchar_t)vkCode);
            return text;
        }
        if (vkCode >= '0' && vkCode <= '9') {
            swprintf_s(text, 32, L"%c", (wchar_t)vkCode);
            return text;
        }
        swprintf_s(text, 32, L"KEY%d", vkCode);
        return text;
    }
}

} // namespace wcf
