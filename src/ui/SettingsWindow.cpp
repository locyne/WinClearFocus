#include "ui/SettingsWindow.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <stdio.h>
#include <string.h>

#include "app/AppContext.h"
#include "app/Application.h"
#include "app/Constants.h"
#include "core/AutoStart.h"
#include "core/ConfigStore.h"
#include "core/HotkeyManager.h"
#include "core/Settings.h"
#include "resource.h"
#include "ui/PreviewWindow.h"
#include "ui/SettingsControls.h"
#include "ui/SettingsRenderer.h"
#include "util/Win32Utils.h"

namespace wcf {
namespace {

using SettingsControls::ComboAutoStartDelay;
using SettingsControls::FormatHotkey;
using SettingsControls::SetHotkeyDisplay;
using SettingsControls::UpdateAutoStartEnabledState;
using SettingsControls::UpdateRegulateScaleLabel;

// 控件句柄由 SettingsControls 模块创建并持有，这里只做别名，方便事件处理。
SettingsControls::Controls& g_controls = SettingsControls::g_controls;

SettingsRenderer::HoverState g_hover;
bool g_trackingMouse = false;

struct HotkeyCaptureState {
    bool active;
    HotkeyAction action;
    HWND button;
    HWND display;
    DWORD startTime;
};

HotkeyCaptureState g_capture = { false, HotkeyAction::Count, NULL, NULL, 0 };

// 录制超时定时器（1 已被虚线预览框占用）
const UINT_PTR kCaptureTimerId = 2;

void CancelHotkeyCapture()
{
    if (!g_capture.active) {
        return;
    }

    if (g_settingsWindow) {
        KillTimer(g_settingsWindow, kCaptureTimerId);
    }

    SetWindowTextW(g_capture.button, L"设置");
    SetHotkeyDisplay(g_capture.display, Hotkey(g_capture.action));

    g_capture.active = false;
    g_capture.action = HotkeyAction::Count;
    g_capture.button = NULL;
    g_capture.display = NULL;
    ReleaseCapture();

    // 录制期间热键被临时注销，取消后立即恢复
    RegisterHotkeys(g_mainWindow);
}

void BeginHotkeyCapture(HotkeyAction action, HWND button, HWND display)
{
    // 点击另一个“设置”按钮时，先把上一个按钮恢复原状
    CancelHotkeyCapture();

    // 录制期间必须注销全局热键，否则系统会吞掉与现有热键相同的按键
    UnregisterHotkeys(g_mainWindow);

    g_capture.active = true;
    g_capture.action = action;
    g_capture.button = button;
    g_capture.display = display;
    g_capture.startTime = GetTickCount();

    SetWindowTextW(button, L"取消");
    SetWindowTextW(display, L"请按下组合键...");
    SetFocus(g_settingsWindow);

    // 10 秒无输入自动取消，避免热键一直停留在注销状态
    if (g_settingsWindow) {
        SetTimer(g_settingsWindow, kCaptureTimerId, 10000, NULL);
    }
}

void ToggleHotkeyCapture(HotkeyAction action, HWND button, HWND display)
{
    if (g_capture.active && g_capture.action == action) {
        CancelHotkeyCapture();
    } else {
        BeginHotkeyCapture(action, button, display);
    }
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_NCCALCSIZE:
        if (wParam == TRUE) {
            return 0;   // 客户区覆盖整个窗口：去掉系统标题栏和外框
        }
        break;

    case WM_NCHITTEST:
    {
        POINT client = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &client);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        if (client.y < SettingsRenderer::kTitleBarHeight &&
            !HitRect(SettingsRenderer::CloseButtonRect(rcClient.right), client.x, client.y)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_NCLBUTTONDBLCLK:
        return 0;   // 双击标题栏不最大化

    case WM_GETMINMAXINFO:
    {
        // 上限和下限都等于当前尺寸：窗口在任何情况下都无法被拉伸/最大化
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = SettingsRenderer::kWindowWidth;
        mmi->ptMinTrackSize.y = SettingsRenderer::kWindowHeight;
        mmi->ptMaxTrackSize.x = SettingsRenderer::kWindowWidth;
        mmi->ptMaxTrackSize.y = SettingsRenderer::kWindowHeight;
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   // 背景在 WM_PAINT 里一次性画完，避免闪烁

    case WM_PAINT:
        SettingsRenderer::Paint(hwnd, g_hover);
        return 0;

    case WM_MOUSEMOVE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const bool hc = HitRect(SettingsRenderer::CloseButtonRect(rc.right), x, y);
        const bool hs = HitRect(SettingsRenderer::SaveButtonRect(rc.right, rc.bottom), x, y);
        const bool hx = HitRect(SettingsRenderer::CancelButtonRect(rc.right, rc.bottom), x, y);
        if (hc != g_hover.close || hs != g_hover.save || hx != g_hover.cancel) {
            g_hover.close = hc;
            g_hover.save = hs;
            g_hover.cancel = hx;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (!g_trackingMouse) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            if (TrackMouseEvent(&tme)) {
                g_trackingMouse = true;
            }
        }
        break;
    }

    case WM_MOUSELEAVE:
        g_trackingMouse = false;
        if (g_hover.close || g_hover.save || g_hover.cancel) {
            g_hover = SettingsRenderer::HoverState();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_LBUTTONUP:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (HitRect(SettingsRenderer::CloseButtonRect(rc.right), x, y)) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (HitRect(SettingsRenderer::SaveButtonRect(rc.right, rc.bottom), x, y)) {
            SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_SAVE_BUTTON, BN_CLICKED), 0);
            return 0;
        }
        if (HitRect(SettingsRenderer::CancelButtonRect(rc.right, rc.bottom), x, y)) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    case WM_CREATE:
        // 设置窗口打开期间保持热键可用；只有开始录制新热键时才临时注销
        g_trackingMouse = false;
        g_hover = SettingsRenderer::HoverState();
        g_capture.active = false;
        g_capture.action = HotkeyAction::Count;

        // 子控件由独立模块创建（布局与自绘辅助都在那里）
        SettingsControls::Create(hwnd);
        break;

    case WM_CTLCOLORSTATIC:
    {
        // 静态文本透明绘制，并使用与自绘背景一致的画刷，避免出现灰色方块
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)SettingsRenderer::BackgroundBrush();
    }

    case WM_CTLCOLORBTN:
        // 复选框等按钮的底色也贴合自绘背景
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (LRESULT)SettingsRenderer::BackgroundBrush();

    case WM_TIMER:
        if (wParam == 1) {
            // 预览框的自动隐藏定时器
            KillTimer(hwnd, 1);
            HideRegulatePreview();
        } else if (wParam == kCaptureTimerId && g_capture.active) {
            // 录制超时：恢复按钮文字并重新注册全局热键
            KillTimer(hwnd, kCaptureTimerId);
            g_capture.active = false;
            SetWindowTextW(g_capture.button, L"设置");
            SetWindowTextW(g_capture.display, L"监听超时，请重试");
            g_capture.action = HotkeyAction::Count;
            g_capture.button = NULL;
            g_capture.display = NULL;
            ReleaseCapture();
            RegisterHotkeys(g_mainWindow);
        }
        break;

    case WM_DESTROY:
        // 预览框与定时器必须随设置窗口一起销毁，避免残影和句柄泄漏
        KillTimer(hwnd, 1);
        KillTimer(hwnd, kCaptureTimerId);
        DestroyPreviewWindow();

        // Tooltip / 权限图标随控件一起释放
        SettingsControls::Cleanup();

        g_settingsWindow = NULL;
        break;

    case WM_HSCROLL:
        if ((HWND)lParam == g_controls.regulateSlider) {
            Settings().regulateScale = ClampInt(
                (int)SendMessageW(g_controls.regulateSlider, TBM_GETPOS, 0, 0), 30, 90);
            UpdateRegulateScaleLabel(hwnd, g_controls.regulateScaleLabel);
            ShowRegulatePreview(hwnd);
        } else if ((HWND)lParam == g_controls.transparencySlider) {
            const int pos = (int)SendMessageW(g_controls.transparencySlider, TBM_GETPOS, 0, 0);
            wchar_t text[50];
            swprintf_s(text, 50, L"透明度步长: %d%%", pos);
            SetWindowTextW(g_controls.transparencyLabel, text);
        }
        break;

    case WM_COMMAND:
        // 固定尺寸输入框内容变化：同步到全局并刷新预览
        if (HIWORD(wParam) == EN_CHANGE &&
            (LOWORD(wParam) == IDC_REGULATE_WIDTH_EDIT ||
             LOWORD(wParam) == IDC_REGULATE_HEIGHT_EDIT)) {
            if (LOWORD(wParam) == IDC_REGULATE_WIDTH_EDIT) {
                Settings().regulateWidth = ClampInt(
                    GetEditInt(g_controls.regulateWidthEdit, Settings().regulateWidth),
                    100, 8000);
            } else {
                Settings().regulateHeight = ClampInt(
                    GetEditInt(g_controls.regulateHeightEdit, Settings().regulateHeight),
                    100, 8000);
            }
            ShowRegulatePreview(hwnd);
            break;
        }

        switch (LOWORD(wParam)) {
        case IDC_TRANSPARENCY_UP_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::TransparencyUp,
                                g_controls.transparencyUpButton,
                                g_controls.transparencyUpDisplay);
            break;
        case IDC_TRANSPARENCY_DOWN_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::TransparencyDown,
                                g_controls.transparencyDownButton,
                                g_controls.transparencyDownDisplay);
            break;
        case IDC_CENTER_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::CenterWindow,
                                g_controls.centerButton, g_controls.centerDisplay);
            break;
        case IDC_SHAKE_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::ShakeWindow,
                                g_controls.shakeButton, g_controls.shakeDisplay);
            break;
        case IDC_RESET_TRANSPARENCY_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::ResetTransparency,
                                g_controls.resetTransparencyButton,
                                g_controls.resetTransparencyDisplay);
            break;
        case IDC_SEND_TO_BOTTOM_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::SendToBottom,
                                g_controls.sendToBottomButton,
                                g_controls.sendToBottomDisplay);
            break;
        case IDC_REGULATE_BUTTON:
            ToggleHotkeyCapture(HotkeyAction::Regulate,
                                g_controls.regulateButton, g_controls.regulateDisplay);
            break;

        case IDC_REGULATE_ENABLE:
            Hotkey(HotkeyAction::Regulate).enabled =
                (SendMessageW(g_controls.regulateEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;

        case IDC_REGULATE_MODE_PERCENT:
        case IDC_REGULATE_MODE_FIXED:
            Settings().regulateMode =
                (LOWORD(wParam) == IDC_REGULATE_MODE_FIXED) ? 1 : 0;
            SendMessageW(g_controls.regulateModePercent, BM_SETCHECK,
                         Settings().regulateMode == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(g_controls.regulateModeFixed, BM_SETCHECK,
                         Settings().regulateMode == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
            SetControlsVisible(g_controls.regulatePercentCtrls, 4,
                               Settings().regulateMode == 0);
            SetControlsVisible(g_controls.regulateFixedCtrls, 6,
                               Settings().regulateMode == 1);
            UpdateRegulateScaleLabel(hwnd, g_controls.regulateScaleLabel);
            ShowRegulatePreview(hwnd);
            break;

        case IDC_REGULATE_CENTER:
            Settings().regulateCenter =
                (SendMessageW(g_controls.regulateCenter, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            ShowRegulatePreview(hwnd);
            break;

        case IDC_REGULATE_IGNORE_TASKBAR:
            Settings().regulateIgnoreTaskbar =
                (SendMessageW(g_controls.regulateIgnoreTaskbar, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            UpdateRegulateScaleLabel(hwnd, g_controls.regulateScaleLabel);
            ShowRegulatePreview(hwnd);
            break;

        case IDC_TRANSPARENCY_ENABLE:
        {
            const BOOL enabled =
                (SendMessageW(g_controls.transparencyEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            Hotkey(HotkeyAction::TransparencyUp).enabled = enabled;
            Hotkey(HotkeyAction::TransparencyDown).enabled = enabled;
            break;
        }

        case IDC_RESET_TRANSPARENCY_ENABLE:
            // 重置透明度是独立开关，不受“启用调整”影响
            Hotkey(HotkeyAction::ResetTransparency).enabled =
                (SendMessageW(g_controls.resetTransparencyEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;

        case IDC_CENTER_IGNORE_TASKBAR:
            Settings().centerIgnoreTaskbar =
                (SendMessageW(g_controls.centerIgnoreTaskbar, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            // 居中位置会随该开关在“工作区/整块屏幕”之间切换，立即刷新虚线预览
            ShowRegulatePreview(hwnd);
            break;

        case IDC_CENTER_ENABLE:
            Hotkey(HotkeyAction::CenterWindow).enabled =
                (SendMessageW(g_controls.centerEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;

        case IDC_SHAKE_ENABLE:
            Hotkey(HotkeyAction::ShakeWindow).enabled =
                (SendMessageW(g_controls.shakeEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;

        case IDC_SEND_TO_BOTTOM_ENABLE:
            Hotkey(HotkeyAction::SendToBottom).enabled =
                (SendMessageW(g_controls.sendToBottomEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;

        case IDC_AUTOSTART:
            UpdateAutoStartEnabledState(g_controls.autoStart, g_controls.autoStartDelay);
            break;

        case IDC_SAVE_BUTTON:
        {
            // 保存前结束可能仍在进行的热键录制，避免热键保持注销状态
            CancelHotkeyCapture();

            AppSettings& s = Settings();

            s.transparencyStep =
                (int)SendMessageW(g_controls.transparencySlider, TBM_GETPOS, 0, 0);
            s.showStartupMessage =
                (SendMessageW(g_controls.showStartupMsg, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            s.centerIgnoreTaskbar =
                (SendMessageW(g_controls.centerIgnoreTaskbar, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;

            // 获取窗口大小调整参数（越界值统一钳制）
            s.regulateMode =
                (SendMessageW(g_controls.regulateModeFixed, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            s.regulateScale = ClampInt(
                (int)SendMessageW(g_controls.regulateSlider, TBM_GETPOS, 0, 0), 30, 90);
            s.regulateIgnoreTaskbar =
                (SendMessageW(g_controls.regulateIgnoreTaskbar, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            s.regulateWidth = ClampInt(
                GetEditInt(g_controls.regulateWidthEdit, s.regulateWidth), 100, 8000);
            s.regulateHeight = ClampInt(
                GetEditInt(g_controls.regulateHeightEdit, s.regulateHeight), 100, 8000);
            s.regulateCenter =
                (SendMessageW(g_controls.regulateCenter, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;

            // 开机自启：先记住勾选状态和下拉框选择
            const BOOL wantAutoStart =
                (SendMessageW(g_controls.autoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
            s.autoStartDelay = ComboAutoStartDelay(g_controls.autoStartDelay);

            SaveConfig();

            // 重新注册热键
            UnregisterHotkeys(g_mainWindow);
            RegisterHotkeys(g_mainWindow);

            // 应用开机自启（写注册表或建计划任务），并把结果一并提示给用户
            wchar_t detail[512] = L"";
            const bool autoStartOk =
                ApplyAutoStart(wantAutoStart, s.autoStartDelay, detail, 512);

            wchar_t msg[1024];
            if (autoStartOk) {
                swprintf_s(msg, 1024, L"设置已保存！\n\n开机自启：%s", detail);
                MessageBoxW(hwnd, msg, L"提示", MB_OK | MB_ICONINFORMATION);
            } else {
                swprintf_s(msg, 1024, L"设置已保存，但开机自启配置失败：\n\n%s", detail);
                MessageBoxW(hwnd, msg, L"警告", MB_OK | MB_ICONWARNING);
            }
            break;
        }

        case IDC_ELEVATE:
            // 以管理员身份重启（当前实例会被关闭）
            RestartAsAdmin();
            break;

        case IDCANCEL:
            // 取消设置时，先结束录制（如有），再重新注册所有热键
            CancelHotkeyCapture();
            RegisterHotkeys(g_mainWindow);
            DestroyWindow(hwnd);
            g_settingsWindow = NULL;
            break;
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_capture.active) {
            // 检查超时（10 秒）
            if (GetTickCount() - g_capture.startTime > 10000) {
                KillTimer(hwnd, kCaptureTimerId);
                g_capture.active = false;
                SetWindowTextW(g_capture.button, L"设置");
                SetWindowTextW(g_capture.display, L"监听超时，请重试");
                g_capture.action = HotkeyAction::Count;
                g_capture.button = NULL;
                g_capture.display = NULL;
                ReleaseCapture();
                RegisterHotkeys(g_mainWindow);
                break;
            }

            // 检测修饰键
            UINT modifiers = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
            if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
            if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) {
                modifiers |= MOD_WIN;
            }

            // 如果没有修饰键，忽略单独的字母键
            if (modifiers == 0 && wParam >= 'A' && wParam <= 'Z') {
                break;
            }

            // 验证热键有效性：必须有修饰键或者是功能键/特殊键
            if (modifiers == 0 &&
                !(wParam >= VK_F1 && wParam <= VK_F24) &&
                wParam != VK_INSERT && wParam != VK_DELETE &&
                wParam != VK_HOME && wParam != VK_END &&
                wParam != VK_PRIOR && wParam != VK_NEXT &&
                wParam != VK_SPACE && wParam != VK_TAB &&
                wParam != VK_RETURN && wParam != VK_ESCAPE) {
                SetWindowTextW(g_capture.display, L"请使用修饰键组合");
                break;
            }

            // 处理功能键、方向键、字母键和数字键
            if ((wParam >= VK_F1 && wParam <= VK_F24) ||
                (wParam >= VK_LEFT && wParam <= VK_DOWN) ||
                (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) ||
                (wParam >= 'A' && wParam <= 'Z') ||
                (wParam >= '0' && wParam <= '9') ||
                wParam == VK_INSERT || wParam == VK_DELETE ||
                wParam == VK_HOME || wParam == VK_END ||
                wParam == VK_PRIOR || wParam == VK_NEXT ||
                wParam == VK_SPACE || wParam == VK_TAB ||
                wParam == VK_RETURN || wParam == VK_ESCAPE) {

                HotkeyBinding& binding = Hotkey(g_capture.action);
                binding.modifiers = modifiers;
                binding.key = (UINT)wParam;

                wchar_t hotkeyText[128];
                FormatHotkey(hotkeyText, 128, binding);
                SetWindowTextW(g_capture.display, hotkeyText);

                // 停止监听：新组合键立即生效，无需等到保存
                KillTimer(hwnd, kCaptureTimerId);
                SetWindowTextW(g_capture.button, L"设置");
                g_capture.active = false;
                g_capture.action = HotkeyAction::Count;
                g_capture.button = NULL;
                g_capture.display = NULL;
                ReleaseCapture();
                RegisterHotkeys(g_mainWindow);
            }
        }
        break;

    case WM_CLOSE:
        // 关闭设置窗口时，先结束录制（如有），再重新注册所有热键
        CancelHotkeyCapture();
        RegisterHotkeys(g_mainWindow);
        DestroyWindow(hwnd);
        g_settingsWindow = NULL;
        break;

    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

} // namespace

void ShowSettingsWindow()
{
    if (g_settingsWindow) {
        SetForegroundWindow(g_settingsWindow);
        return;
    }

    // 注册设置窗口类（重复注册会失败，忽略即可）
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = WCF_SETTINGS_WINDOW_CLASS;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;      // 背景完全自绘
    wc.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_WINCLEARFOCUS));
    RegisterClassW(&wc);

    // 计算窗口居中位置
    const int windowWidth = SettingsRenderer::kWindowWidth;
    const int windowHeight = SettingsRenderer::kWindowHeight;
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screenWidth - windowWidth) / 2;
    const int y = (screenHeight - windowHeight) / 2;

    // 无边框窗口；标题栏、圆角和底部按钮由自绘/DWM 提供
    wchar_t windowTitle[100];
    swprintf_s(windowTitle, 100, L"WinClearFocus %s 设置", WCF_APP_VERSION);
    g_settingsWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        WCF_SETTINGS_WINDOW_CLASS,
        windowTitle,
        WS_POPUP | WS_CLIPCHILDREN,
        x, y,
        windowWidth, windowHeight,
        NULL, NULL, GetModuleHandleW(NULL), NULL);

    if (g_settingsWindow) {
        // Windows 11：DWM 圆角（Win10 上调用失败无副作用）
        DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(g_settingsWindow, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &corner, sizeof(corner));

        ShowWindow(g_settingsWindow, SW_SHOW);
        UpdateWindow(g_settingsWindow);
    }
}

} // namespace wcf
