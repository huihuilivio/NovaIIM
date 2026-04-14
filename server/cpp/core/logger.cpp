#include "core/logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <mutex>
#include <vector>

namespace nova::log {

// 所有 logger 共享的 sinks 和 pattern，在 Init() 中创建
static std::vector<spdlog::sink_ptr> g_sinks;
static std::string g_pattern;
static std::once_flag g_init_flag;

void Init(const LogOptions& opts) {
    std::call_once(g_init_flag, [&] {
        // 控制台 sink（带颜色）
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        g_sinks.push_back(console_sink);

        // 可选：文件轮转 sink
        if (!opts.file.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                opts.file, opts.max_size, opts.max_files, opts.rotate_on_open);
            g_sinks.push_back(file_sink);
        }

        // 先移除 spdlog 自动创建的默认 logger，再注册新的
        spdlog::drop_all();
        auto default_logger = std::make_shared<spdlog::logger>("nova", g_sinks.begin(), g_sinks.end());
        default_logger->set_level(opts.level);
        default_logger->set_pattern(opts.pattern);
        g_pattern = opts.pattern;
        spdlog::set_default_logger(default_logger);
        spdlog::flush_on(opts.flush_level);
    });
}

std::shared_ptr<spdlog::logger> Get(const std::string& name) {
    // 先从 registry 查找
    auto logger = spdlog::get(name);
    if (logger) {
        return logger;
    }

    // 首次调用：用共享 sinks 创建，继承默认 logger 的级别
    logger = std::make_shared<spdlog::logger>(name, g_sinks.begin(), g_sinks.end());
    logger->set_level(spdlog::default_logger()->level());
    logger->set_pattern(g_pattern);
    spdlog::register_logger(logger);
    return logger;
}

}  // namespace nova::log
