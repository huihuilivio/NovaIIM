#pragma once
// 自动检测运行平台和设备标识

#include <string>

namespace nova::client {

/// 检测平台类型: "pc", "mobile", "tablet", "web"
std::string DetectDeviceType();

/// 生成稳定的设备唯一标识（基于机器名/硬件信息）
std::string GenerateDeviceId();

}  // namespace nova::client
