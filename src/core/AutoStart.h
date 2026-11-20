#pragma once

#include <windows.h>

namespace wcf {

// 注册表启动项或计划任务是否存在。
bool IsAutoStartRegistered();

// 应用开机自启设置。
// 普通用户写 HKCU\...\Run；管理员创建登录触发的计划任务（可延迟）。
// detail 返回给用户看的说明文本。
bool ApplyAutoStart(BOOL enable, int delaySeconds, wchar_t* detail, size_t detailLen);

} // namespace wcf
