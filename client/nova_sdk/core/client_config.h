#pragma once
// 客户端配置

#include <cstdint>
#include <string>

namespace nova::client {

struct ClientConfig {
    // 服务器连接
    std::string server_host = "127.0.0.1";
    uint16_t server_port    = 9090;

    // 设备信息
    std::string device_id;    // 设备唯一标识
    std::string device_type;  // "pc", "mobile", "web"

    // 心跳（应小于服务端超时的 1/2）
    uint32_t heartbeat_interval_ms = 10000;  // 10 秒

    // 重连
    uint32_t reconnect_initial_ms  = 1000;   // 初始重连间隔 1s
    uint32_t reconnect_max_ms      = 30000;  // 最大重连间隔 30s
    double   reconnect_multiplier  = 2.0;    // 退避倍数

    // 请求超时
    uint32_t request_timeout_ms = 10000;     // 10 秒

    // 本地存储
    std::string data_dir;   // 本地数据目录

    // 日志
    std::string log_level = "info";
    std::string log_file;   // 空=仅控制台
};

/// 从 YAML 文件加载配置（可选，失败返回 false）
bool LoadClientConfig(ClientConfig& cfg, const std::string& path);

}  // namespace nova::client
