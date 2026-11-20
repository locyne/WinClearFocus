#pragma once

#include <windows.h>

namespace wcf {

// 注册主窗口类并创建隐藏的主窗口。失败返回 false。
bool InitializeMainWindow(HINSTANCE hInstance);

} // namespace wcf
