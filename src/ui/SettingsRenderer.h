#pragma once

#include <windows.h>

namespace wcf {
namespace SettingsRenderer {

constexpr int kTitleBarHeight = 36;
constexpr int kWindowWidth = 460;
constexpr int kWindowHeight = 720;   // 紧凑排版，适配 1366x768

struct HoverState {
    bool close;
    bool save;
    bool cancel;

    HoverState() : close(false), save(false), cancel(false) {}
};

HBRUSH BackgroundBrush();

RECT CloseButtonRect(int clientWidth);
RECT SaveButtonRect(int clientWidth, int clientHeight);
RECT CancelButtonRect(int clientWidth, int clientHeight);

// GDI+ 双缓冲绘制整个设置窗口。
void Paint(HWND hwnd, const HoverState& hover);

} // namespace SettingsRenderer
} // namespace wcf
