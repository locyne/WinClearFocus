#pragma once

#include <windows.h>

namespace wcf {

// 修饰键组合 -> 显示文本（如 "CONTROL+ALT+SHIFT"）。
// 返回内部静态缓冲区，调用方应尽快使用，不要长期保存指针。
const wchar_t* ModifierName(UINT modifiers);

// 虚拟键码 -> 显示文本（如 "UP"、"F5"、"C"）。
// 返回内部静态缓冲区，调用方应尽快使用，不要长期保存指针。
const wchar_t* KeyName(UINT vkCode);

} // namespace wcf
