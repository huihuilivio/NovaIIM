#pragma once

// NovaIIM 配置系统 —— 从 YAML 文件加载（基于 ylt/struct_yaml）
//
// C++20 聚合类型自动反射，无需 YLT_REFL 宏
//
// 用法:
//   Config cfg;
//   if (!LoadConfig(cfg, "server.yaml")) { return 1; }
//   int port = cfg.server.port;

#include <string>

namespace nova {

struct ServerConfig {
    int         port           = 9090;
    int         worker_threads = 4;
    int         queue_capacity = 8192;   // MPMC 队列容量，必须为 2 的幂
};

struct LogConfig {
    std::string level          = "debug";   // trace/debug/info/warn/error/critical
    std::string pattern        = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] [%s:%#] %v";
    std::string flush_level    = "warn";     // 达到此级别自动刷盘
    std::string file;                        // 空则不写文件
    int         max_size       = 10;         // 单文件最大 MB
    int         max_files      = 3;          // 保留文件数
    bool        rotate_on_open = false;      // 启动时立即轮转
};

struct DatabaseConfig {
    std::string host     = "127.0.0.1";
    int         port     = 3306;
    std::string user     = "root";
    std::string password;
    std::string dbname   = "nova_im";
    int         pool_size = 4;
};

struct Config {
    ServerConfig   server;
    LogConfig      log;
    DatabaseConfig db;
};

// 从 YAML 文件加载配置，成功返回 true
bool LoadConfig(Config& cfg, const std::string& path);

}  // namespace nova
