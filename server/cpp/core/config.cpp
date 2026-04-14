#include "core/config.h"

#include <ylt/struct_yaml/yaml_reader.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nova {

bool LoadConfig(Config& cfg, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr, "Config file not found: %s\n", path.c_str());
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

    return true;
}

}  // namespace nova
