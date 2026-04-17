#pragma once

#include "app_config.h"

namespace nova {

// 应用生命周期管理
// 职责：初始化各子系统、启动服务、等待退出信号、有序关闭
// main() 只负责 CLI 解析，将配置传入 Run()
class Application {
public:
    // 运行服务端，阻塞直到收到 SIGINT/SIGTERM
    // 返回进程退出码
    static int Run(const AppConfig& cfg);
};

}  // namespace nova
