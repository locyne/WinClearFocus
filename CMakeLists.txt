cmake_minimum_required(VERSION 4.3)
project(WinClearFocus LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW 默认按 ANSI 编译（UNICODE 未定义，TCHAR 展开为 char），
# 而本项目源码全部使用宽字符 API，必须显式定义 UNICODE / _UNICODE，
# 让 windows.h 把 MessageBox、CreateWindow 等映射到 ...W 版本。
add_compile_definitions(UNICODE _UNICODE)

include_directories(WinClearFocus)

# WIN32 关键字生成 GUI 子系统程序（MinGW 下等价于 -mwindows），不会弹出控制台黑框
add_executable(WinClearFocus WIN32
        WinClearFocus/resource.h
        WinClearFocus/WinClearFocus.cpp
        WinClearFocus/WinClearFocus.ico
        WinClearFocus/WinClearFocus.manifest
        WinClearFocus/WinClearFocus.rc)

# 清单改了要让 .rc 重新编译（windres 不会自动感知这个依赖）
set_source_files_properties(WinClearFocus/WinClearFocus.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/WinClearFocus/WinClearFocus.manifest")

# 链接 Windows 系统库
target_link_libraries(WinClearFocus PRIVATE user32 shell32 comctl32 gdiplus dwmapi)
