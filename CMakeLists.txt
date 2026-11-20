cmake_minimum_required(VERSION 3.20)
project(WinClearFocus VERSION 1.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW 默认按 ANSI 编译（UNICODE 未定义，TCHAR 展开为 char），
# 而本项目源码全部使用宽字符 API，必须显式定义 UNICODE / _UNICODE，
# 让 windows.h 把 MessageBox、CreateWindow 等映射到 ...W 版本。
add_compile_definitions(UNICODE _UNICODE)

set(WCF_SOURCES
        src/main.cpp

        src/app/AppContext.cpp
        src/app/Application.cpp

        src/core/AutoStart.cpp
        src/core/ConfigStore.cpp
        src/core/HotkeyManager.cpp
        src/core/Settings.cpp
        src/core/WindowActions.cpp

        src/ui/MainWindow.cpp
        src/ui/PreviewWindow.cpp
        src/ui/SettingsControls.cpp
        src/ui/SettingsRenderer.cpp
        src/ui/SettingsWindow.cpp
        src/ui/TrayIcon.cpp

        src/util/HotkeyText.cpp
        src/util/Win32Utils.cpp
)

# WIN32 关键字生成 GUI 子系统程序（MinGW 下等价于 -mwindows），不会弹出控制台黑框
add_executable(WinClearFocus WIN32
        ${WCF_SOURCES}
        res/resource.h
        res/WinClearFocus.rc
        res/WinClearFocus.ico
        res/WinClearFocus.manifest
)

target_include_directories(WinClearFocus PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/res
)

# 清单改了要让 .rc 重新编译（windres 不会自动感知这个依赖）
set_source_files_properties(res/WinClearFocus.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/WinClearFocus.manifest")

# 静态链接 GCC 运行库与 winpthread，生成单文件绿色 exe：
# 否则在没装 MinGW 的机器上会缺少 libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll
target_link_options(WinClearFocus PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
)

# 链接 Windows 系统库
target_link_libraries(WinClearFocus PRIVATE user32 shell32 comctl32 gdiplus dwmapi)

# ===== 单元测试（纯逻辑，不依赖 GUI） =====
include(CTest)

if (BUILD_TESTING)
    add_executable(WinClearFocusTests
            tests/test_main.cpp
            src/core/Settings.cpp
            src/util/HotkeyText.cpp
            src/util/Win32Utils.cpp
    )
    target_include_directories(WinClearFocusTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(WinClearFocusTests PRIVATE user32)
    add_test(NAME WinClearFocusTests COMMAND WinClearFocusTests)
endif()
