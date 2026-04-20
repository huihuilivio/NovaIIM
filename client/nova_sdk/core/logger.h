#pragma once
// 客户端日志封装 — spdlog 单例

#include <export.h>

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace nova::client {

class NOVA_SDK_API Logger {
public:
    /// 初始化日志（可选文件输出）
    /// @param name     logger 名称
    /// @param log_file 日志文件路径（空=仅控制台）
    /// @param level    日志级别 ("trace","debug","info","warn","error")
    static void Init(const std::string& name = "nova_sdk",
                     const std::string& log_file = "",
                     const std::string& level = "info");

    /// 获取 logger 实例
    static std::shared_ptr<spdlog::logger>& Get();

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace nova::client

// 便捷宏 — 直接调用 logger->log()，避免 SPDLOG_LOGGER_* 的 FMT_STRING 命名空间冲突
#define NOVA_LOG_TRACE(...)    ::nova::client::Logger::Get()->trace(__VA_ARGS__)
#define NOVA_LOG_DEBUG(...)    ::nova::client::Logger::Get()->debug(__VA_ARGS__)
#define NOVA_LOG_INFO(...)     ::nova::client::Logger::Get()->info(__VA_ARGS__)
#define NOVA_LOG_WARN(...)     ::nova::client::Logger::Get()->warn(__VA_ARGS__)
#define NOVA_LOG_ERROR(...)    ::nova::client::Logger::Get()->error(__VA_ARGS__)
