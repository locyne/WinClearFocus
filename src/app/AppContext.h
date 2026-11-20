#pragma once

#include <windows.h>

namespace wcf {

// 顶层窗口句柄。主窗口隐藏，仅用于接收热键和托盘消息；
// 设置窗口与预览框按需创建。
extern HWND g_mainWindow;
extern HWND g_settingsWindow;
extern HWND g_previewWindow;

// 判断句柄是否属于本程序（避免热键误操作自己）。
bool IsOwnWindow(HWND hwnd);

} // namespace wcf
