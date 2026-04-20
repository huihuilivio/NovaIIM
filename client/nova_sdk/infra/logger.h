#pragma once

// NovaIIM SDK 日志系统 —— 封装 spdlog
//
// 用法:
//   NOVA_LOG_DEBUG("user {} connected", uid);               // 默认 logger
//   NOVA_NLOG_DEBUG("Net", "user {} connected", uid);       // 命名 logger
//
// 初始化 (main 中调用一次):
//   nova::log::Init();                                      // 默认 debug 级别
//   nova::log::Init({.level = spdlog::level::info});        // 指定级别

// SPDLOG_ACTIVE_LEVEL 必须在 spdlog 头文件之前定义，
// 以启用 SPDLOG_TRACE / SPDLOG_DEBUG 等编译期过滤。
// 通过 CMake compile definition 设置，此处提供兜底。
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace nova::log {

struct LogOptions {
    spdlog::level::level_enum level       = spdlog::level::debug;
    spdlog::level::level_enum flush_level = spdlog::level::warn;
    std::string pattern                   = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] [%s:%#] %v";
    std::string file;                          // 空则不写文件
    std::size_t max_size  = 10 * 1024 * 1024;  // 单文件最大字节
    std::size_t max_files = 3;                 // 保留文件数
    bool rotate_on_open   = false;             // 启动时立即轮转
};

// 初始化日志系统（控制台 + 可选文件轮转）
void Init(const LogOptions& opts = {});

// 获取命名 logger（首次调用自动创建，后续从 registry 缓存返回）
std::shared_ptr<spdlog::logger> Get(const std::string& name);

}  // namespace nova::log

// ============================================================
// 默认 logger 宏（带源码位置：文件名、行号、函数名）
// ============================================================
#define NOVA_LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define NOVA_LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define NOVA_LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define NOVA_LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define NOVA_LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define NOVA_LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

// ============================================================
// 命名 logger 宏（第一个参数为 logger 名称）
// ============================================================
#define NOVA_NLOG_TRACE(name, ...)    SPDLOG_LOGGER_TRACE(::nova::log::Get(name), __VA_ARGS__)
#define NOVA_NLOG_DEBUG(name, ...)    SPDLOG_LOGGER_DEBUG(::nova::log::Get(name), __VA_ARGS__)
#define NOVA_NLOG_INFO(name, ...)     SPDLOG_LOGGER_INFO(::nova::log::Get(name), __VA_ARGS__)
#define NOVA_NLOG_WARN(name, ...)     SPDLOG_LOGGER_WARN(::nova::log::Get(name), __VA_ARGS__)
#define NOVA_NLOG_ERROR(name, ...)    SPDLOG_LOGGER_ERROR(::nova::log::Get(name), __VA_ARGS__)
#define NOVA_NLOG_CRITICAL(name, ...) SPDLOG_LOGGER_CRITICAL(::nova::log::Get(name), __VA_ARGS__)
