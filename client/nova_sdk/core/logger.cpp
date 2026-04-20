#include "logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace nova::client {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::Init(const std::string& name,
                  const std::string& log_file,
                  const std::string& level) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    if (!log_file.empty()) {
        // 10 MB 轮转, 最多 3 个文件
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, 10 * 1024 * 1024, 3));
    }

    logger_ = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%s:%#] %v");
    logger_->set_level(spdlog::level::from_str(level));
    logger_->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(logger_);
}

std::shared_ptr<spdlog::logger>& Logger::Get() {
    if (!logger_) {
        // 懒初始化：未调用 Init 时使用默认 console logger
        logger_ = spdlog::stdout_color_mt("nova_sdk");
    }
    return logger_;
}

}  // namespace nova::client
