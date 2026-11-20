#pragma once

#include <windows.h>

#include "core/Settings.h"

namespace wcf {

// 按当前设置注册所有已启用的全局热键（会先注销旧热键）。
void RegisterHotkeys(HWND hwnd);

// 注销本程序使用的全部热键 ID。
void UnregisterHotkeys(HWND hwnd);

// 动作 <-> RegisterHotKey 使用的整数 ID 互转。
int HotkeyId(HotkeyAction action);
bool HotkeyActionFromId(int id, HotkeyAction* action);

} // namespace wcf
