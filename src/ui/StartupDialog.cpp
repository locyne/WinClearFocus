#include "ui/StartupDialog.h"

#include <stdio.h>

#include "app/Constants.h"
#include "resource.h"

namespace wcf {
namespace {

HICON g_bigIcon = NULL;
HICON g_smallIcon = NULL;

INT_PTR CALLBACK StartupDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_INITDIALOG:
    {
        // 标题栏：程序图标 + 英文名 + 版本
        SetWindowTextW(hDlg, L"WinClearFocus " WCF_APP_VERSION);

        g_bigIcon = (HICON)LoadImageW(
            GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_WINCLEARFOCUS), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
        g_smallIcon = (HICON)LoadImageW(
            GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_WINCLEARFOCUS), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

        if (g_bigIcon) {
            SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_bigIcon);
        }
        if (g_smallIcon) {
            SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_smallIcon);
        }

        wchar_t line1[128];
        swprintf_s(line1, 128, L"%s已在系统托盘运行。", WCF_APP_NAME_ZH);
        SetDlgItemTextW(hDlg, IDC_STARTUP_LINE1, line1);
        SetDlgItemTextW(hDlg, IDC_STARTUP_LINE2, L"右键点击托盘图标进行设置。");
        SetDlgItemTextW(hDlg, IDC_STARTUP_LINE3, L"提示：命令行加 -silent 可静默启动");
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

} // namespace

void ShowStartupDialog(HINSTANCE hInstance, HWND owner)
{
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_STARTUP_DIALOG), owner,
                    StartupDialogProc, 0);

    // 对话框关闭后再释放标题栏图标
    if (g_bigIcon) {
        DestroyIcon(g_bigIcon);
        g_bigIcon = NULL;
    }
    if (g_smallIcon) {
        DestroyIcon(g_smallIcon);
        g_smallIcon = NULL;
    }
}

} // namespace wcf
