#pragma once
// 客户端日志封装 — spdlog 单例

#include <core/export.h>

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace nova::client {

class NOVA_CLIENT_API Logger {
public:
    /// 初始化日志（可选文件输出）
    /// @param name     logger 名称
    /// @param log_file 日志文件路径（空=仅控制台）
    /// @param level    日志级别 ("trace","debug","info","warn","error")
    static void Init(const std::string& name = "nova_client",
                     const std::string& log_file = "",
                     const std::string& level = "info");

    /// 获取 logger 实例
    static std::shared_ptr<spdlog::logger>& Get();

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace nova::client

// 便捷宏
#define NOVA_LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(::nova::client::Logger::Get(), __VA_ARGS__)
#define NOVA_LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(::nova::client::Logger::Get(), __VA_ARGS__)
#define NOVA_LOG_INFO(...)     SPDLOG_LOGGER_INFO(::nova::client::Logger::Get(), __VA_ARGS__)
#define NOVA_LOG_WARN(...)     SPDLOG_LOGGER_WARN(::nova::client::Logger::Get(), __VA_ARGS__)
#define NOVA_LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(::nova::client::Logger::Get(), __VA_ARGS__)
