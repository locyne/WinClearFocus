#include "app/AppContext.h"

namespace wcf {

HWND g_mainWindow = NULL;
HWND g_settingsWindow = NULL;
HWND g_previewWindow = NULL;

bool IsOwnWindow(HWND hwnd)
{
    return hwnd == g_mainWindow || hwnd == g_settingsWindow || hwnd == g_previewWindow;
}

} // namespace wcf
