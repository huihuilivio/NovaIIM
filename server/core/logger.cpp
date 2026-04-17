#include "core/logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <mutex>
#include <vector>

namespace nova::log {

static constexpr const char* kDefaultPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v";

// 所有 logger 共享的 sinks 和 pattern
static std::vector<spdlog::sink_ptr> g_sinks;
static std::string g_pattern;
static std::once_flag g_setup_flag;   // Init() 或 EnsureDefaults() 中首先触发的一方生效
static std::mutex g_get_mutex;

// 确保至少有默认 sink 和 pattern（惰性初始化，供 Init() 未被调用时使用）
static void EnsureDefaults() {
    std::call_once(g_setup_flag, [] {
        g_sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        g_pattern = kDefaultPattern;
    });
}

void Init(const LogOptions& opts) {
    std::call_once(g_setup_flag, [&] {
        // 控制台 sink（带颜色）
        g_sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

        // 可选：文件轮转 sink
        if (!opts.file.empty()) {
            g_sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                opts.file, opts.max_size, opts.max_files, opts.rotate_on_open));
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
    EnsureDefaults();

    // 先从 registry 查找（spdlog::get 内部有锁，线程安全）
    auto logger = spdlog::get(name);
    if (logger) return logger;

    // 首次创建需要加锁，防止两个线程同时为同一 name 创建
    std::lock_guard<std::mutex> lock(g_get_mutex);

    // double-check: 可能在等锁期间已被其他线程创建
    logger = spdlog::get(name);
    if (logger) return logger;

    logger = std::make_shared<spdlog::logger>(name, g_sinks.begin(), g_sinks.end());
    logger->set_level(spdlog::default_logger()->level());
    logger->set_pattern(g_pattern);
    spdlog::register_logger(logger);
    return logger;
}

}  // namespace nova::log
