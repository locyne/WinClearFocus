#pragma once

#include <windows.h>

namespace wcf {
namespace TrayIcon {

void Create(HWND owner);
void Remove();
void ShowContextMenu(HWND owner);

} // namespace TrayIcon
} // namespace wcf
