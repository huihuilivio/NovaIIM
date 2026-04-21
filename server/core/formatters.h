#pragma once

// NovaIIM std::formatter 特化集中定义
//
// spdlog 配置了 SPDLOG_USE_STD_FORMAT，使用 std::format 而非 fmt。
// 自定义类型通过特化 std::formatter 实现格式化输出。
// 所有业务结构体的 formatter 统一放在此文件，便于扩展。

#include "core/app_config.h"

#include <ylt/struct_yaml/yaml_writer.h>

#include <format>
#include <string>
#include <string_view>

// ---- AppConfig ----
template <>
struct std::formatter<nova::AppConfig> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const nova::AppConfig& cfg, FormatContext& ctx) const {
        std::string yaml;
        iguana::to_yaml(cfg, yaml);
        // 去掉末尾多余换行
        while (!yaml.empty() && yaml.back() == '\n') {
            yaml.pop_back();
        }
        return std::formatter<std::string_view>::format(yaml, ctx);
    }
};
