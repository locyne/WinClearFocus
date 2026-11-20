#pragma once

#include <windows.h>

namespace wcf {
namespace TrayIcon {

void Create(HWND owner);

// explorer.exe 重启后会广播 TaskbarCreated，用于重新添加托盘图标。
void Recreate(HWND owner);

void Remove();
void ShowContextMenu(HWND owner);

} // namespace TrayIcon
} // namespace wcf
