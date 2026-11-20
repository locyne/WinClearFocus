#pragma once

#include <windows.h>

#include <stddef.h>

#include "core/Settings.h"

namespace wcf {
namespace SettingsControls {

// 设置窗口的全部子控件句柄。由 Create() 填充，窗口销毁后仍保留到下次重建。
struct Controls {
    HWND transparencySlider;
    HWND transparencyLabel;
    HWND transparencyUpDisplay;
    HWND transparencyDownDisplay;
    HWND centerDisplay;
    HWND shakeDisplay;
    HWND transparencyUpButton;
    HWND transparencyDownButton;
    HWND centerButton;
    HWND shakeButton;
    HWND transparencyEnable;
    HWND resetTransparencyEnable;
    HWND centerEnable;
    HWND shakeEnable;
    HWND showStartupMsg;
    HWND centerIgnoreTaskbar;
    HWND adminStatus;
    HWND elevateButton;
    HWND resetTransparencyDisplay;
    HWND resetTransparencyButton;
    HWND sendToBottomEnable;
    HWND sendToBottomDisplay;
    HWND sendToBottomButton;
    HWND autoStart;
    HWND autoStartDelay;
    HWND regulateEnable;
    HWND regulateDisplay;
    HWND regulateButton;
    HWND regulateModePercent;
    HWND regulateModeFixed;
    HWND regulateSlider;
    HWND regulateScaleLabel;
    HWND regulateIgnoreTaskbar;
    HWND regulateWidthEdit;
    HWND regulateHeightEdit;
    HWND regulateHint;
    HWND regulateCenter;
    HWND regulatePercentCtrls[4];
    HWND regulateFixedCtrls[6];
};

extern Controls g_controls;

// 创建全部子控件并统一字体。parent 为设置窗口句柄。
void Create(HWND parent);

// 释放与控件一起创建的资源（Tooltip、问号图标）。
void Cleanup();

// 热键文本格式化。
void FormatHotkey(wchar_t* buffer, size_t length, const HotkeyBinding& binding);
void SetHotkeyDisplay(HWND display, const HotkeyBinding& binding);

// “当前：W × H px”标签与自启下拉框状态。
void UpdateRegulateScaleLabel(HWND hSettings, HWND hLabel);
void UpdateAutoStartEnabledState(HWND check, HWND combo);
int ComboAutoStartDelay(HWND combo);

} // namespace SettingsControls
} // namespace wcf
