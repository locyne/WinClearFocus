#pragma once

#include <windows.h>

namespace wcf {

// exe 所在目录下的 config.ini 绝对路径。
// 计划任务启动时工作目录可能是 System32，因此必须使用绝对路径。
const wchar_t* ConfigPath();

// 从 config.ini 载入设置；文件不存在时自动写入默认配置。
void LoadConfig();

// 把当前设置写回 config.ini。
void SaveConfig();

} // namespace wcf
