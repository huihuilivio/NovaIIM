#include "core/app_config.h"

#include <ylt/struct_yaml/yaml_reader.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nova {

bool LoadConfig(AppConfig& cfg, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr, "AppConfig file not found: %s\n", path.c_str());
        return false;
    }

    std::ifstream ifs(path);
    if (!ifs) {
        std::fprintf(stderr, "Failed to open config: %s\n", path.c_str());
        return false;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string yaml_str = oss.str();

    std::error_code ec;
    iguana::from_yaml(cfg, yaml_str, ec);
    if (ec) {
        std::fprintf(stderr, "Failed to parse config: %s\n", ec.message().c_str());
        return false;
    }

    // 基本配置校验
    if (cfg.server.port <= 0 || cfg.server.port > 65535) {
        std::fprintf(stderr, "server.port must be in range 1-65535, got %d\n", cfg.server.port);
        return false;
    }
    if (cfg.server.ws_port < 0 || cfg.server.ws_port > 65535) {
        std::fprintf(stderr, "server.ws_port must be in range 0-65535, got %d\n", cfg.server.ws_port);
        return false;
    }
    if (cfg.server.ws_port > 0 && cfg.server.ws_port == cfg.server.port) {
        std::fprintf(stderr, "server.ws_port must differ from server.port\n");
        return false;
    }
    if (cfg.server.ws_port > 0 && cfg.admin.enabled && cfg.server.ws_port == cfg.admin.port) {
        std::fprintf(stderr, "server.ws_port must differ from admin.port\n");
        return false;
    }
    if (cfg.admin.enabled) {
        if (cfg.admin.port <= 0 || cfg.admin.port > 65535) {
            std::fprintf(stderr, "admin.port must be in range 1-65535, got %d\n", cfg.admin.port);
            return false;
        }
        if (cfg.admin.port == cfg.server.port) {
            std::fprintf(stderr, "admin.port must differ from server.port\n");
            return false;
        }
        if (cfg.admin.jwt_secret.empty()) {
            std::fprintf(stderr, "admin.enabled but jwt_secret is empty\n");
            return false;
        }
    }
    if (cfg.db.type == "mysql" && cfg.db.pool_size <= 0) {
        std::fprintf(stderr, "db.pool_size must be > 0, got %d\n", cfg.db.pool_size);
        return false;
    }

    return true;
}

}  // namespace nova
