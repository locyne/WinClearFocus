#include "ui/SettingsRenderer.h"

#include <gdiplus.h>

#include "resource.h"

namespace wcf {
namespace SettingsRenderer {
namespace {

const COLORREF kSettingsBg = RGB(243, 243, 243);

// 标题栏左侧的小图标。静态加载一次，随进程存活。
HICON TitleIcon()
{
    static HICON icon = (HICON)LoadImageW(
        GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_WINCLEARFOCUS), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    return icon;
}

void FillRoundedRect(Gdiplus::Graphics& g, const RECT& r, int radius,
                     const Gdiplus::Color& color)
{
    Gdiplus::GraphicsPath path;
    const int d = radius * 2;
    path.AddArc(r.left, r.top, d, d, 180.0f, 90.0f);
    path.AddArc(r.right - d, r.top, d, d, 270.0f, 90.0f);
    path.AddArc(r.right - d, r.bottom - d, d, d, 0.0f, 90.0f);
    path.AddArc(r.left, r.bottom - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
    Gdiplus::SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

void DrawCenteredText(Gdiplus::Graphics& g, const wchar_t* text, const RECT& r,
                      Gdiplus::Font& font, const Gdiplus::Color& color)
{
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::RectF box((Gdiplus::REAL)r.left, (Gdiplus::REAL)r.top,
                       (Gdiplus::REAL)(r.right - r.left),
                       (Gdiplus::REAL)(r.bottom - r.top));
    g.DrawString(text, -1, &font, box, &fmt, &brush);
}

} // namespace

HBRUSH BackgroundBrush()
{
    static HBRUSH brush = CreateSolidBrush(kSettingsBg);
    return brush;
}

RECT CloseButtonRect(int clientWidth)
{
    // 28×28 热区，垂直居中于 32px 标题栏
    RECT r = { clientWidth - 36, 2, clientWidth - 8, 30 };
    return r;
}

RECT SaveButtonRect(int clientWidth, int clientHeight)
{
    RECT r = { clientWidth - 16 - 108, clientHeight - 16 - 36, clientWidth - 16, clientHeight - 16 };
    return r;
}

RECT CancelButtonRect(int clientWidth, int clientHeight)
{
    RECT r = SaveButtonRect(clientWidth, clientHeight);
    r.right = r.left - 12;
    r.left = r.right - 88;
    return r;
}

void Paint(HWND hwnd, const HoverState& hover)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    const int w = rc.right;
    const int h = rc.bottom;
    if (w <= 0 || h <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // 背景
        Gdiplus::SolidBrush bg(Gdiplus::Color(243, 243, 243));
        g.FillRectangle(&bg, 0, 0, w, h);

        // 标题：小图标在左（绘制于 GDI+ 之后），文字从图标右侧开始
        Gdiplus::Font titleFont(L"Microsoft YaHei UI", 13.0f,
                                Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush titleBrush(Gdiplus::Color(32, 32, 32));
        g.DrawString(L"WinClearFocus 设置", -1, &titleFont,
                     Gdiplus::PointF(40.0f, 7.0f), &titleBrush);

        // 关闭按钮：圆形，悬停变红
        const RECT cb = CloseButtonRect(w);
        const int cx = (cb.left + cb.right) / 2;
        const int cy = (cb.top + cb.bottom) / 2;
        const int cr = 13;
        if (hover.close) {
            Gdiplus::SolidBrush red(Gdiplus::Color(196, 43, 28));
            g.FillEllipse(&red, cx - cr, cy - cr, cr * 2, cr * 2);
        }
        Gdiplus::Pen cross(hover.close ? Gdiplus::Color(255, 255, 255)
                                       : Gdiplus::Color(80, 80, 80), 1.6f);
        g.DrawLine(&cross, (Gdiplus::REAL)(cx - 4), (Gdiplus::REAL)(cy - 4),
                   (Gdiplus::REAL)(cx + 4), (Gdiplus::REAL)(cy + 4));
        g.DrawLine(&cross, (Gdiplus::REAL)(cx - 4), (Gdiplus::REAL)(cy + 4),
                   (Gdiplus::REAL)(cx + 4), (Gdiplus::REAL)(cy - 4));

        // 底部分隔线
        Gdiplus::Pen sep(Gdiplus::Color(226, 226, 226), 1.0f);
        g.DrawLine(&sep, 0.0f, (Gdiplus::REAL)(h - 58),
                   (Gdiplus::REAL)w, (Gdiplus::REAL)(h - 58));

        // 底部按钮（圆角，替代原生按钮）
        Gdiplus::Font btnFont(L"Microsoft YaHei UI", 15.0f,
                              Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        const RECT sb = SaveButtonRect(w, h);
        FillRoundedRect(g, sb, 6, hover.save ? Gdiplus::Color(0, 90, 168)
                                             : Gdiplus::Color(0, 103, 192));
        DrawCenteredText(g, L"保存设置", sb, btnFont, Gdiplus::Color(255, 255, 255));

        const RECT xb = CancelButtonRect(w, h);
        FillRoundedRect(g, xb, 6, hover.cancel ? Gdiplus::Color(214, 214, 214)
                                               : Gdiplus::Color(229, 229, 229));
        DrawCenteredText(g, L"关闭", xb, btnFont, Gdiplus::Color(32, 32, 32));
    }

    // GDI+ 绘制结束后再叠加标题栏小图标
    HICON icon = TitleIcon();
    if (icon) {
        const int iconW = GetSystemMetrics(SM_CXSMICON);
        const int iconH = GetSystemMetrics(SM_CYSMICON);
        DrawIconEx(memDC, 16, (kTitleBarHeight - iconH) / 2, icon,
                   iconW, iconH, 0, NULL, DI_NORMAL);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

} // namespace SettingsRenderer
} // namespace wcf
