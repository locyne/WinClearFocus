#pragma once

#include <windows.h>

#include "core/Settings.h"

namespace wcf {

// 当前前台窗口；取不到时退回桌面窗口的第一个子窗口。
HWND GetTopMostWindow();

// 透明度读写（alpha 0~255）。
void SetWindowTransparency(HWND hwnd, int alpha);
int GetWindowTransparency(HWND hwnd);

// 具体窗口操作。
void AdjustWindowTransparency(bool increase);
void ResetWindowTransparency();
void CenterWindow();
void ShakeWindow();
void SendWindowToBottom();
void RegulateWindowSize();

// 目标窗口是否最大化或全屏。
bool IsMaximizedOrFullscreen(HWND hwnd);

// 根据当前设置计算目标矩形（屏幕坐标）。
// hwnd 仅用于确定所在显示器。
bool ComputeRegulatedRect(HWND hwnd, RECT* out);

// 根据热键动作执行对应操作。
void PerformHotkeyAction(HotkeyAction action);

} // namespace wcf
