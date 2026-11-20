#pragma once

#include <windows.h>

namespace wcf {

// 显示启动提示对话框（模态）。标题栏显示程序图标 + 英文名 + 版本。
void ShowStartupDialog(HINSTANCE hInstance, HWND owner);

} // namespace wcf
