#include "client_config.h"
#include <infra/device_info.h>
#include <infra/logger.h>

#include <fstream>

#include <ylt/struct_yaml/yaml_reader.h>

namespace nova::client {

/// 配置文件加载后自动填充未指定的设备信息
static void AutoFillDeviceInfo(ClientConfig& cfg) {
    if (cfg.device_type.empty()) {
        cfg.device_type = DetectDeviceType();
    }
    if (cfg.device_id.empty()) {
        cfg.device_id = GenerateDeviceId();
    }
}

bool LoadClientConfig(ClientConfig& cfg, const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        NOVA_LOG_ERROR("Failed to open config file: {}", path);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());

    std::error_code ec;
    struct_yaml::from_yaml(cfg, content, ec);
    if (ec) {
        NOVA_LOG_ERROR("Failed to parse config: {}", ec.message());
        return false;
    }

    AutoFillDeviceInfo(cfg);
    return true;
}

}  // namespace nova::client
