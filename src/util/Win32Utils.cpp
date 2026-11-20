#include "util/Win32Utils.h"

#include <stdlib.h>

namespace wcf {

int ClampInt(int value, int lo, int hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

bool HitRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

int GetEditInt(HWND edit, int fallback)
{
    wchar_t buf[32] = { 0 };
    if (!edit || !GetWindowTextW(edit, buf, 32)) {
        return fallback;
    }
    if (!buf[0]) {
        return fallback;
    }
    return (int)wcstol(buf, NULL, 10);
}

void SetControlsVisible(HWND* controls, int count, bool visible)
{
    for (int i = 0; i < count; ++i) {
        if (controls[i]) {
            ShowWindow(controls[i], visible ? SW_SHOW : SW_HIDE);
        }
    }
}

} // namespace wcf
