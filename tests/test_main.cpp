#include <windows.h>

#include <stdio.h>
#include <wchar.h>

#include "core/Settings.h"
#include "util/HotkeyText.h"
#include "util/Win32Utils.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        printf("FAIL: %s (%s:%d)\n", expression, file, line);
        ++g_failures;
    }
}

#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)

void TestClampInt()
{
    CHECK(wcf::ClampInt(-5, 0, 10) == 0);
    CHECK(wcf::ClampInt(0, 0, 10) == 0);
    CHECK(wcf::ClampInt(5, 0, 10) == 5);
    CHECK(wcf::ClampInt(10, 0, 10) == 10);
    CHECK(wcf::ClampInt(15, 0, 10) == 10);
}

void TestHitRect()
{
    const RECT r = { 10, 20, 30, 40 };
    CHECK(wcf::HitRect(r, 10, 20));
    CHECK(wcf::HitRect(r, 29, 39));
    CHECK(!wcf::HitRect(r, 30, 39));
    CHECK(!wcf::HitRect(r, 29, 40));
    CHECK(!wcf::HitRect(r, 9, 20));
    CHECK(!wcf::HitRect(r, 10, 19));
}

void TestHotkeyText()
{
    CHECK(wcscmp(wcf::ModifierName(0), L"") == 0);
    CHECK(wcscmp(wcf::ModifierName(MOD_ALT | MOD_SHIFT), L"ALT+SHIFT") == 0);
    CHECK(wcscmp(wcf::ModifierName(MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN),
                 L"CONTROL+ALT+SHIFT+WIN") == 0);

    CHECK(wcscmp(wcf::KeyName('C'), L"C") == 0);
    CHECK(wcscmp(wcf::KeyName('7'), L"7") == 0);
    CHECK(wcscmp(wcf::KeyName(VK_LEFT), L"LEFT") == 0);
    CHECK(wcscmp(wcf::KeyName(VK_NEXT), L"PAGEDOWN") == 0);
    CHECK(wcscmp(wcf::KeyName(VK_F1), L"F1") == 0);
    CHECK(wcscmp(wcf::KeyName(VK_F13), L"F13") == 0);
    CHECK(wcscmp(wcf::KeyName(VK_F24), L"F24") == 0);
    CHECK(wcscmp(wcf::KeyName(0xE8), L"KEY232") == 0);
}

void TestDefaultSettings()
{
    const wcf::AppSettings& s = wcf::Settings();

    CHECK(s.transparencyStep == 10);
    CHECK(s.showStartupMessage == 1);
    CHECK(s.autoStartDelay == 0);
    CHECK(s.centerIgnoreTaskbar == 1);
    CHECK(s.regulateMode == 0);
    CHECK(s.regulateScale == 70);
    CHECK(s.regulateIgnoreTaskbar == 1);
    CHECK(s.regulateWidth == 1280);
    CHECK(s.regulateHeight == 720);
    CHECK(s.regulateCenter == 1);

    CHECK(wcf::Hotkey(wcf::HotkeyAction::CenterWindow).enabled == TRUE);
    CHECK(wcf::Hotkey(wcf::HotkeyAction::Regulate).enabled == FALSE);
    CHECK(wcf::Hotkey(wcf::HotkeyAction::CenterWindow).key == 'C');
    CHECK(wcf::Hotkey(wcf::HotkeyAction::TransparencyUp).key == VK_RIGHT);
    CHECK(wcf::Hotkey(wcf::HotkeyAction::SendToBottom).key == 'Z');
    CHECK(wcf::Hotkey(wcf::HotkeyAction::Regulate).key == 'R');
}

} // namespace

int main()
{
    TestClampInt();
    TestHitRect();
    TestHotkeyText();
    TestDefaultSettings();

    if (g_failures == 0) {
        printf("All WinClearFocus tests passed.\n");
        return 0;
    }

    printf("%d test(s) failed.\n", g_failures);
    return 1;
}
