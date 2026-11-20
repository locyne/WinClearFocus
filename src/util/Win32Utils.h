#pragma once

#include <windows.h>

namespace wcf {

int ClampInt(int value, int lo, int hi);

bool HitRect(const RECT& rect, int x, int y);

// 读取 EDIT 控件里的整数；为空或读取失败时返回 fallback。
int GetEditInt(HWND edit, int fallback);

// 批量显示/隐藏控件。
void SetControlsVisible(HWND* controls, int count, bool visible);

} // namespace wcf
