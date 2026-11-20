#pragma once

// ===== 应用信息 =====
#define WCF_APP_VERSION L"v1.2.0"

// ===== 窗口类名 =====
#define WCF_MAIN_WINDOW_CLASS     L"WinClearFocusClass"
#define WCF_SETTINGS_WINDOW_CLASS L"SettingsWindowClass"
#define WCF_PREVIEW_WINDOW_CLASS  L"WinClearFocusPreview"

// ===== 托盘 =====
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT     1001
#define ID_TRAY_SETTINGS 1002

// ===== 全局热键 ID（顺序必须与 HotkeyAction 枚举一致） =====
#define ID_HOTKEY_TRANSPARENCY_UP   1
#define ID_HOTKEY_TRANSPARENCY_DOWN 2
#define ID_HOTKEY_CENTER_WINDOW     3
#define ID_HOTKEY_SHAKE_WINDOW      4
#define ID_HOTKEY_RESET_TRANSPARENCY 5
#define ID_HOTKEY_SEND_TO_BOTTOM    6
#define ID_HOTKEY_REGULATE          7

// ===== 设置窗口控件 ID =====
#define IDC_TRANSPARENCY_UP_BUTTON      2001
#define IDC_TRANSPARENCY_DOWN_BUTTON    2002
#define IDC_CENTER_BUTTON               2003
#define IDC_SHAKE_BUTTON                2004
#define IDC_TRANSPARENCY_SLIDER         2005
#define IDC_SAVE_BUTTON                 2006
#define IDC_TRANSPARENCY_LABEL          2007
#define IDC_TRANSPARENCY_UP_DISPLAY     2008
#define IDC_TRANSPARENCY_DOWN_DISPLAY   2009
#define IDC_CENTER_DISPLAY              2010
#define IDC_SHAKE_DISPLAY               2011
#define IDC_TRANSPARENCY_ENABLE         2012
#define IDC_CENTER_ENABLE               2013
#define IDC_SHAKE_ENABLE                2014
#define IDC_SHOW_STARTUP_MSG            2015
#define IDC_ADMIN_STATUS                2016
#define IDC_ELEVATE                     2017
#define IDC_PERMISSION_HELP             2018
#define IDC_CENTER_IGNORE_TASKBAR       2019
#define IDC_RESET_TRANSPARENCY_DISPLAY  2020
#define IDC_RESET_TRANSPARENCY_BUTTON   2021
#define IDC_SEND_TO_BOTTOM_ENABLE       2022
#define IDC_SEND_TO_BOTTOM_DISPLAY      2023
#define IDC_SEND_TO_BOTTOM_BUTTON       2024
#define IDC_AUTOSTART                   2025
#define IDC_AUTOSTART_DELAY             2026
#define IDC_REGULATE_ENABLE             2027
#define IDC_REGULATE_DISPLAY            2028
#define IDC_REGULATE_BUTTON             2029
#define IDC_REGULATE_MODE_PERCENT       2030
#define IDC_REGULATE_MODE_FIXED         2031
#define IDC_REGULATE_SLIDER             2032
#define IDC_REGULATE_SCALE_LABEL        2033
#define IDC_REGULATE_IGNORE_TASKBAR     2034
#define IDC_REGULATE_WIDTH_EDIT         2035
#define IDC_REGULATE_HEIGHT_EDIT        2036
#define IDC_REGULATE_HINT               2037
#define IDC_REGULATE_CENTER             2038
#define IDC_RESET_TRANSPARENCY_ENABLE   2039
