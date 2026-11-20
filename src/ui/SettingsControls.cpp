#include "ui/SettingsControls.h"

#include <commctrl.h>

#include <stdio.h>
#include <string.h>

#include "app/Application.h"
#include "app/Constants.h"
#include "core/AutoStart.h"
#include "core/Settings.h"
#include "core/WindowActions.h"
#include "resource.h"
#include "ui/SettingsRenderer.h"
#include "util/HotkeyText.h"
#include "util/Win32Utils.h"

namespace wcf {
namespace SettingsControls {
namespace {

HFONT g_uiFont = NULL;
HWND g_permissionTip = NULL;
HICON g_permissionIcon = NULL;
bool g_permissionIconOwned = false;

// 必须是静态存储：TTM_ADDTOOL 只保存指针，不会拷贝字符串内容。
wchar_t g_permissionTipText[] =
    L"提示：部分软件是以管理员权限运行的，普通权限下的本程序无法控制这类软件。"
    L"若需控制它们，请点击“去提权”以管理员身份重启本程序。";

BOOL CALLBACK SetChildFontProc(HWND child, LPARAM lParam)
{
    SendMessageW(child, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

void EnsureUiFont()
{
    if (g_uiFont) {
        return;
    }

    // 统一字体：Microsoft YaHei UI 12px（≈9pt），与 GDI+ 绘制的标题/按钮保持一致。
    // 不显式指定时标准控件会退回 16px 的 SYSTEM_FONT，中文标签会被撑大并裁切。
    LOGFONTW lf = { 0 };
    lf.lfHeight = -12;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    g_uiFont = CreateFontIndirectW(&lf);
    if (!g_uiFont) {
        g_uiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
}

} // namespace

Controls g_controls;

void FormatHotkey(wchar_t* buffer, size_t length, const HotkeyBinding& binding)
{
    swprintf_s(buffer, length, L"%s+%s",
               ModifierName(binding.modifiers), KeyName(binding.key));
}

void SetHotkeyDisplay(HWND display, const HotkeyBinding& binding)
{
    wchar_t text[128];
    FormatHotkey(text, 128, binding);
    SetWindowTextW(display, text);
}

void UpdateRegulateScaleLabel(HWND hSettings, HWND hLabel)
{
    RECT target;
    if (!hLabel || !hSettings || !ComputeRegulatedRect(hSettings, &target)) {
        return;
    }
    wchar_t text[128];
    swprintf_s(text, 128, L"当前：%d × %d px",
               target.right - target.left, target.bottom - target.top);
    SetWindowTextW(hLabel, text);
}

int ComboAutoStartDelay(HWND combo)
{
    static const int kDelays[] = { 0, 5, 10, 15, 30 };
    const int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)(sizeof(kDelays) / sizeof(kDelays[0]))) {
        return 0;
    }
    return kDelays[sel];
}

// 只有“勾选 + 管理员”才允许选延迟
void UpdateAutoStartEnabledState(HWND check, HWND combo)
{
    const BOOL on = (SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(combo, on && IsRunningAsAdmin());
}

void Create(HWND parent)
{
    EnsureUiFont();

    // 权限判断只做一次：UI 布局（延迟下拉框是否可用）和自启逻辑都要用
    const BOOL isAdmin = IsRunningAsAdmin();
    const AppSettings& s = Settings();

    // 列宽按该字体实测留余量：
    // “增加透明度:”63px、“当前权限：普通用户”108px、“CONTROL+ALT+SHIFT+PAGEDOWN”约 192px
    const int labelX = 20;        // 分区标题、热键标签
    const int labelWidth = 110;   // 中文标签宽度
    const int fieldX = 135;       // 热键显示框 / 复选框的 X 坐标
    const int fieldWidth = 205;   // 热键显示框：完整名称也不会被截断
    const int fieldHeight = 24;
    const int buttonX = 350;      // “设置”按钮
    const int buttonWidth = 60;

    int yPos = 10 + SettingsRenderer::kTitleBarHeight;
    wchar_t keyText[128];

    // ============ 第一块：窗口居中 ============
    CreateWindowW(L"STATIC", L"窗口居中:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.centerEnable = CreateWindowW(
        L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX, yPos, 60, 22,
        parent, (HMENU)IDC_CENTER_ENABLE, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.centerEnable, BM_SETCHECK,
                 Hotkey(HotkeyAction::CenterWindow).enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 26;

    CreateWindowW(L"STATIC", L"热键:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.centerDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_CENTER_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::CenterWindow));
    SetWindowTextW(g_controls.centerDisplay, keyText);

    g_controls.centerButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_CENTER_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 38;

    // ============ 第二块：窗口置底 ============
    CreateWindowW(L"STATIC", L"窗口置底:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.sendToBottomEnable = CreateWindowW(
        L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX, yPos, 60, 22,
        parent, (HMENU)IDC_SEND_TO_BOTTOM_ENABLE, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.sendToBottomEnable, BM_SETCHECK,
                 Hotkey(HotkeyAction::SendToBottom).enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 26;

    CreateWindowW(L"STATIC", L"热键:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.sendToBottomDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_SEND_TO_BOTTOM_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::SendToBottom));
    SetWindowTextW(g_controls.sendToBottomDisplay, keyText);

    g_controls.sendToBottomButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_SEND_TO_BOTTOM_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 38;

    // ============ 第三块：窗口抖动 ============
    CreateWindowW(L"STATIC", L"窗口抖动:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.shakeEnable = CreateWindowW(
        L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX, yPos, 60, 22,
        parent, (HMENU)IDC_SHAKE_ENABLE, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.shakeEnable, BM_SETCHECK,
                 Hotkey(HotkeyAction::ShakeWindow).enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 26;

    CreateWindowW(L"STATIC", L"热键:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.shakeDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_SHAKE_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::ShakeWindow));
    SetWindowTextW(g_controls.shakeDisplay, keyText);

    g_controls.shakeButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_SHAKE_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 38;

    // ============ 第四块：窗口大小调整 ============
    CreateWindowW(L"STATIC", L"窗口大小调整:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, 120, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.regulateEnable = CreateWindowW(
        L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX, yPos, 60, 22,
        parent, (HMENU)IDC_REGULATE_ENABLE, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.regulateEnable, BM_SETCHECK,
                 Hotkey(HotkeyAction::Regulate).enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 26;

    CreateWindowW(L"STATIC", L"热键:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.regulateDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_REGULATE_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::Regulate));
    SetWindowTextW(g_controls.regulateDisplay, keyText);

    g_controls.regulateButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_REGULATE_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 28;

    g_controls.regulateModePercent = CreateWindowW(
        L"BUTTON", L"比例缩放",
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
        fieldX, yPos, 90, 22,
        parent, (HMENU)IDC_REGULATE_MODE_PERCENT, GetModuleHandleW(NULL), NULL);
    g_controls.regulateModeFixed = CreateWindowW(
        L"BUTTON", L"固定尺寸", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        fieldX + 105, yPos, 90, 22,
        parent, (HMENU)IDC_REGULATE_MODE_FIXED, GetModuleHandleW(NULL), NULL);

    g_controls.regulateCenter = CreateWindowW(
        L"BUTTON", L"调整后居中", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX + 200, yPos, 120, 22,
        parent, (HMENU)IDC_REGULATE_CENTER, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.regulateCenter, BM_SETCHECK,
                 s.regulateCenter ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_controls.regulateModePercent, BM_SETCHECK,
                 s.regulateMode == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_controls.regulateModeFixed, BM_SETCHECK,
                 s.regulateMode == 1 ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 30;

    // ---- 比例缩放模式 ----
    g_controls.regulatePercentCtrls[0] = CreateWindowW(
        L"STATIC", L"缩放比例:", WS_VISIBLE | WS_CHILD,
        labelX, yPos + 4, 85, 22,
        parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.regulateSlider = CreateWindowW(
        TRACKBAR_CLASSW, L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        fieldX - 10, yPos, 200, 30,
        parent, (HMENU)IDC_REGULATE_SLIDER, GetModuleHandleW(NULL), NULL);
    g_controls.regulatePercentCtrls[1] = g_controls.regulateSlider;

    SendMessageW(g_controls.regulateSlider, TBM_SETRANGE, TRUE, MAKELONG(30, 90));
    SendMessageW(g_controls.regulateSlider, TBM_SETPOS, TRUE,
                 ClampInt(s.regulateScale, 30, 90));
    SendMessageW(g_controls.regulateSlider, TBM_SETTICFREQ, 10, 0);

    g_controls.regulateScaleLabel = CreateWindowW(
        L"STATIC", L"", WS_VISIBLE | WS_CHILD,
        fieldX + 195, yPos + 6, 125, 22,
        parent, (HMENU)IDC_REGULATE_SCALE_LABEL, GetModuleHandleW(NULL), NULL);
    g_controls.regulatePercentCtrls[2] = g_controls.regulateScaleLabel;

    g_controls.regulateIgnoreTaskbar = CreateWindowW(
        L"BUTTON", L"缩放时忽略任务栏", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX, yPos + 32, 200, 22,
        parent, (HMENU)IDC_REGULATE_IGNORE_TASKBAR, GetModuleHandleW(NULL), NULL);
    g_controls.regulatePercentCtrls[3] = g_controls.regulateIgnoreTaskbar;
    SendMessageW(g_controls.regulateIgnoreTaskbar, BM_SETCHECK,
                 s.regulateIgnoreTaskbar ? BST_CHECKED : BST_UNCHECKED, 0);

    // ---- 固定尺寸模式 ----
    g_controls.regulateFixedCtrls[0] = CreateWindowW(
        L"STATIC", L"尺寸:", WS_VISIBLE | WS_CHILD,
        labelX, yPos + 2, 60, 22,
        parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.regulateWidthEdit = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
        fieldX, yPos, 70, 24,
        parent, (HMENU)IDC_REGULATE_WIDTH_EDIT, GetModuleHandleW(NULL), NULL);
    g_controls.regulateFixedCtrls[1] = g_controls.regulateWidthEdit;

    g_controls.regulateFixedCtrls[2] = CreateWindowW(
        L"STATIC", L"×", WS_VISIBLE | WS_CHILD,
        fieldX + 78, yPos + 2, 16, 22,
        parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.regulateHeightEdit = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
        fieldX + 98, yPos, 70, 24,
        parent, (HMENU)IDC_REGULATE_HEIGHT_EDIT, GetModuleHandleW(NULL), NULL);
    g_controls.regulateFixedCtrls[3] = g_controls.regulateHeightEdit;

    g_controls.regulateFixedCtrls[4] = CreateWindowW(
        L"STATIC", L"px", WS_VISIBLE | WS_CHILD,
        fieldX + 173, yPos + 2, 26, 22,
        parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.regulateHint = CreateWindowW(
        L"STATIC", L"（超出当前显示器时会等比缩小并居中）",
        WS_VISIBLE | WS_CHILD,
        fieldX, yPos + 32, 315, 20,
        parent, (HMENU)IDC_REGULATE_HINT, GetModuleHandleW(NULL), NULL);
    g_controls.regulateFixedCtrls[5] = g_controls.regulateHint;

    {
        wchar_t num[16];
        swprintf_s(num, 16, L"%d", s.regulateWidth);
        SetWindowTextW(g_controls.regulateWidthEdit, num);
        swprintf_s(num, 16, L"%d", s.regulateHeight);
        SetWindowTextW(g_controls.regulateHeightEdit, num);
    }

    SetControlsVisible(g_controls.regulatePercentCtrls, 4, s.regulateMode == 0);
    SetControlsVisible(g_controls.regulateFixedCtrls, 6, s.regulateMode == 1);
    UpdateRegulateScaleLabel(parent, g_controls.regulateScaleLabel);

    yPos += 64;

    // ============ 第五块：透明度控制 ============
    CreateWindowW(L"STATIC", L"透明度控制:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    // “启用调整”控制增加/减少透明度这对热键；
    // “启用重置”是独立开关：即使调整功能关闭，也能用重置热键把误调透明的窗口救回来。
    g_controls.transparencyEnable = CreateWindowW(
        L"BUTTON", L"启用调整", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX, yPos, 75, 22,
        parent, (HMENU)IDC_TRANSPARENCY_ENABLE, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.transparencyEnable, BM_SETCHECK,
                 (Hotkey(HotkeyAction::TransparencyUp).enabled ||
                  Hotkey(HotkeyAction::TransparencyDown).enabled)
                     ? BST_CHECKED : BST_UNCHECKED, 0);

    g_controls.resetTransparencyEnable = CreateWindowW(
        L"BUTTON", L"启用重置", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        fieldX + 85, yPos, 75, 22,
        parent, (HMENU)IDC_RESET_TRANSPARENCY_ENABLE, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.resetTransparencyEnable, BM_SETCHECK,
                 Hotkey(HotkeyAction::ResetTransparency).enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 26;

    // 透明度增加热键
    CreateWindowW(L"STATIC", L"增加透明度:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.transparencyUpDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_TRANSPARENCY_UP_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::TransparencyUp));
    SetWindowTextW(g_controls.transparencyUpDisplay, keyText);

    g_controls.transparencyUpButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_TRANSPARENCY_UP_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 26;

    // 透明度减少热键
    CreateWindowW(L"STATIC", L"减少透明度:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.transparencyDownDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_TRANSPARENCY_DOWN_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::TransparencyDown));
    SetWindowTextW(g_controls.transparencyDownDisplay, keyText);

    g_controls.transparencyDownButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_TRANSPARENCY_DOWN_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 26;

    // 重置透明度热键
    CreateWindowW(L"STATIC", L"重置透明度:", WS_VISIBLE | WS_CHILD,
                  labelX, yPos, labelWidth, fieldHeight,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.resetTransparencyDisplay = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        fieldX, yPos, fieldWidth, fieldHeight,
        parent, (HMENU)IDC_RESET_TRANSPARENCY_DISPLAY, GetModuleHandleW(NULL), NULL);
    FormatHotkey(keyText, 128, Hotkey(HotkeyAction::ResetTransparency));
    SetWindowTextW(g_controls.resetTransparencyDisplay, keyText);

    g_controls.resetTransparencyButton = CreateWindowW(
        L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        buttonX, yPos, buttonWidth, fieldHeight,
        parent, (HMENU)IDC_RESET_TRANSPARENCY_BUTTON, GetModuleHandleW(NULL), NULL);

    yPos += 28;

    // ============ 第六块：透明度步长 ============
    wchar_t stepText[64];
    swprintf_s(stepText, 64, L"透明度步长: %d%%", s.transparencyStep);
    g_controls.transparencyLabel = CreateWindowW(
        L"STATIC", stepText, WS_VISIBLE | WS_CHILD,
        labelX, yPos, 220, 22,
        parent, (HMENU)IDC_TRANSPARENCY_LABEL, GetModuleHandleW(NULL), NULL);

    yPos += 22;

    g_controls.transparencySlider = CreateWindowW(
        TRACKBAR_CLASSW, L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        labelX, yPos, 390, 30,
        parent, (HMENU)IDC_TRANSPARENCY_SLIDER, GetModuleHandleW(NULL), NULL);

    SendMessageW(g_controls.transparencySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 50));
    SendMessageW(g_controls.transparencySlider, TBM_SETPOS, TRUE, s.transparencyStep);
    SendMessageW(g_controls.transparencySlider, TBM_SETTICFREQ, 5, 0);

    yPos += 40;

    // ============ 第七块：行为开关（启动提示 / 居中选项） ============
    g_controls.showStartupMsg = CreateWindowW(
        L"BUTTON", L"启动成功后显示提示框",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        labelX, yPos, 205, 22,
        parent, (HMENU)IDC_SHOW_STARTUP_MSG, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.showStartupMsg, BM_SETCHECK,
                 s.showStartupMessage ? BST_CHECKED : BST_UNCHECKED, 0);

    g_controls.centerIgnoreTaskbar = CreateWindowW(
        L"BUTTON", L"居中时忽略任务栏", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        labelX + 220, yPos, 205, 22,
        parent, (HMENU)IDC_CENTER_IGNORE_TASKBAR, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.centerIgnoreTaskbar, BM_SETCHECK,
                 s.centerIgnoreTaskbar ? BST_CHECKED : BST_UNCHECKED, 0);

    yPos += 34;

    // ============ 第八块：开机自启（含延迟） ============
    const BOOL autoStartOn = IsAutoStartRegistered();
    g_controls.autoStart = CreateWindowW(
        L"BUTTON", L"开机自动启动（静默）",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        labelX, yPos + 2, 150, 22,
        parent, (HMENU)IDC_AUTOSTART, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.autoStart, BM_SETCHECK,
                 autoStartOn ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowW(L"STATIC", L"延迟:", WS_VISIBLE | WS_CHILD,
                  labelX + 155, yPos + 2, 40, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    g_controls.autoStartDelay = CreateWindowW(
        L"COMBOBOX", NULL,
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        labelX + 195, yPos, 90, 200,
        parent, (HMENU)IDC_AUTOSTART_DELAY, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_controls.autoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"未选择");
    SendMessageW(g_controls.autoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"5");
    SendMessageW(g_controls.autoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"10");
    SendMessageW(g_controls.autoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"15");
    SendMessageW(g_controls.autoStartDelay, CB_ADDSTRING, 0, (LPARAM)L"30");

    {
        const int delays[] = { 0, 5, 10, 15, 30 };
        int sel = 0;
        for (int i = 0; i < 5; ++i) {
            if (delays[i] == s.autoStartDelay) {
                sel = i;
                break;
            }
        }
        SendMessageW(g_controls.autoStartDelay, CB_SETCURSEL, sel, 0);
    }

    CreateWindowW(L"STATIC",
                  isAdmin ? L"（登录后延迟启动）" : L"（延迟需管理员权限）",
                  WS_VISIBLE | WS_CHILD,
                  labelX + 292, yPos + 2, 150, 22,
                  parent, NULL, GetModuleHandleW(NULL), NULL);

    // 普通用户写注册表 Run 无法设置延迟，直接置灰下拉框
    UpdateAutoStartEnabledState(g_controls.autoStart, g_controls.autoStartDelay);

    yPos += 38;

    // ============ 第九块：当前权限状态 ============
    g_controls.adminStatus = CreateWindowW(
        L"STATIC", L"", WS_VISIBLE | WS_CHILD,
        labelX, yPos, 200, fieldHeight,
        parent, (HMENU)IDC_ADMIN_STATUS, GetModuleHandleW(NULL), NULL);

    g_controls.elevateButton = CreateWindowW(
        L"BUTTON", L"去提权", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        labelX + 210, yPos, 60, fieldHeight,
        parent, (HMENU)IDC_ELEVATE, GetModuleHandleW(NULL), NULL);

    SetWindowTextW(g_controls.adminStatus,
                   isAdmin ? L"当前权限：管理员" : L"当前权限：普通用户");

    // “权限说明”问号图标：LoadIcon 取系统自带图标，再复制成 16×16，
    // 直接用 32×32 的系统图标会在 24px 高的行里被裁切
    HICON hSystemQuestion = LoadIconW(NULL, IDI_QUESTION);
    g_permissionIcon = (HICON)CopyImage(hSystemQuestion, IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON),
                                        GetSystemMetrics(SM_CYSMICON), 0);
    g_permissionIconOwned = (g_permissionIcon != NULL);
    if (!g_permissionIcon) {
        g_permissionIcon = hSystemQuestion;   // 复制失败时退回共享图标（不能销毁）
    }

    // 用 BS_ICON 按钮而不是 SS_ICON 静态框：静态框对鼠标是“透明”的，
    // 鼠标消息会直接落到父窗口，TTF_SUBCLASS 收不到悬停事件。
    HWND hPermissionHelp = CreateWindowW(
        L"BUTTON", L"",
        WS_VISIBLE | WS_CHILD | BS_ICON | BS_CENTER | BS_VCENTER,
        labelX + 276, yPos + 1, 22, 22,
        parent, (HMENU)IDC_PERMISSION_HELP, GetModuleHandleW(NULL), NULL);
    SendMessageW(hPermissionHelp, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_permissionIcon);

    // 原生 Tooltip：TTF_SUBCLASS 让 tooltip 自己接管图标的鼠标悬停跟踪
    g_permissionTip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, NULL, GetModuleHandleW(NULL), NULL);
    if (g_permissionTip) {
        TOOLINFO ti = { 0 };
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = parent;
        ti.uId = (UINT_PTR)hPermissionHelp;
        ti.lpszText = g_permissionTipText;

        // 没有 v6 清单时进程用的是老版 comctl32（5.82），
        // 它只接受旧版 TOOLINFO 大小，传 sizeof(TOOLINFO) 会返回 FALSE；
        // 带 v6 清单时又必须传完整大小。这里先按完整大小添加，失败再退回旧大小。
        if (!SendMessageW(g_permissionTip, TTM_ADDTOOLW, 0, (LPARAM)&ti)) {
            ti.cbSize = TTTOOLINFOW_V1_SIZE;
            SendMessageW(g_permissionTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
        }
        // 长文本按 380px 自动折行，否则会拉成一条超宽横幅
        SendMessageW(g_permissionTip, TTM_SETMAXTIPWIDTH, 0, 380);
        // 悬停 500ms 弹出（默认偏慢）；移开后 100ms 即可再次提示
        SendMessageW(g_permissionTip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELONG(500, 0));
        SendMessageW(g_permissionTip, TTM_SETDELAYTIME, TTDT_RESHOW, MAKELONG(100, 0));
    }

    if (isAdmin) {
        // 已经是管理员，提权按钮和说明图标都没有意义
        ShowWindow(g_controls.elevateButton, SW_HIDE);
        ShowWindow(hPermissionHelp, SW_HIDE);
    }

    // 统一设置字体（放在最后，一次性覆盖所有已创建的控件）
    EnumChildWindows(parent, SetChildFontProc, (LPARAM)g_uiFont);
}

void Cleanup()
{
    if (g_permissionTip) {
        DestroyWindow(g_permissionTip);
        g_permissionTip = NULL;
    }
    if (g_permissionIcon && g_permissionIconOwned) {
        DestroyIcon(g_permissionIcon);
    }
    g_permissionIcon = NULL;
    g_permissionIconOwned = false;
}

} // namespace SettingsControls
} // namespace wcf
