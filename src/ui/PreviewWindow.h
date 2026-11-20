#pragma once

#include <windows.h>

namespace wcf {

// 在设置窗口所在显示器上显示虚线预览框；500ms 后由设置窗口的 WM_TIMER 收起。
void ShowRegulatePreview(HWND hSettings);
void HideRegulatePreview();

// 设置窗口销毁时一并销毁预览窗口。
void DestroyPreviewWindow();

} // namespace wcf
