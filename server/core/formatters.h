#pragma once

// NovaIIM fmt::formatter 特化集中定义
//
// spdlog 使用 bundled fmt，自定义类型通过特化 fmt::formatter 实现格式化输出。
// 所有业务结构体的 formatter 统一放在此文件，便于扩展。

#include "core/config.h"

#include <spdlog/fmt/fmt.h>
#include <ylt/struct_yaml/yaml_writer.h>

#include <string>
#include <string_view>

// ---- Config ----
template <>
struct fmt::formatter<nova::Config> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const nova::Config& cfg, FormatContext& ctx) const {
        std::string yaml;
        iguana::to_yaml(cfg, yaml);
        // 去掉末尾多余换行
        while (!yaml.empty() && yaml.back() == '\n') {
            yaml.pop_back();
        }
        return fmt::formatter<std::string_view>::format(yaml, ctx);
    }
};
