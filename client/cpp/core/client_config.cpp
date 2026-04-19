#include "client_config.h"
#include "logger.h"

#include <fstream>

#include <ylt/struct_yaml/yaml_reader.h>

namespace nova::client {

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
    return true;
}

}  // namespace nova::client
