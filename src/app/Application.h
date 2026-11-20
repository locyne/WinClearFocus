#pragma once

namespace wcf {

// 当前进程是否以管理员权限运行。
bool IsRunningAsAdmin();

// 以管理员身份重启自己（会先清理托盘、热键和窗口）。
void RestartAsAdmin();

} // namespace wcf
