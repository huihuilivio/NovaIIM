#include "device_info.h"

#include <sstream>
#include <cstdint>

// ---- 平台头文件 ----
#if defined(_WIN32)
#include <Windows.h>
#elif defined(__ANDROID__)
#include <sys/system_properties.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#endif

namespace nova::client {

// ================================================================
//  DetectDeviceType
// ================================================================

#if defined(__linux__) && !defined(__ANDROID__)
static std::string DetectLinuxDistro() {
    // 解析 /etc/os-release 中的 ID 字段
    std::ifstream ifs("/etc/os-release");
    if (ifs.is_open()) {
        std::string line;
        while (std::getline(ifs, line)) {
            // 匹配 ID=xxx（不含 ID_LIKE）
            if (line.size() > 3 && line[0] == 'I' && line[1] == 'D' && line[2] == '=') {
                auto val = line.substr(3);
                // 去除引号
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }
                if (!val.empty()) return val;  // ubuntu, centos, debian, fedora, arch ...
            }
        }
    }
    return "linux";
}
#endif

std::string DetectDeviceType() {
#if defined(__ANDROID__)
    return "android";
#elif defined(__APPLE__)
  #if TARGET_OS_IOS
    return "ios";
  #elif TARGET_OS_TV
    return "tvos";
  #elif TARGET_OS_WATCH
    return "watchos";
  #else
    return "macos";
  #endif
#elif defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return DetectLinuxDistro();
#elif defined(__EMSCRIPTEN__)
    return "web";
#else
    return "unknown";
#endif
}

// ================================================================
//  GenerateDeviceId — 平台相关的稳定设备标识
// ================================================================

#if defined(_WIN32)

static std::string PlatformDeviceId() {
    // 机器名 + 用户名
    char computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD csize = sizeof(computer);
    GetComputerNameA(computer, &csize);

    char user[256] = {};
    DWORD usize = sizeof(user);
    GetUserNameA(user, &usize);

    return std::string(computer) + "-" + user;
}

#elif defined(__ANDROID__)

static std::string PlatformDeviceId() {
    char buf[PROP_VALUE_MAX] = {};
    // ro.serialno 是设备序列号，大多数设备可用
    __system_property_get("ro.serialno", buf);
    if (buf[0]) return buf;

    // 回退到 ro.boot.serialno
    __system_property_get("ro.boot.serialno", buf);
    if (buf[0]) return buf;

    // 最终回退
    __system_property_get("ro.product.model", buf);
    return buf[0] ? std::string(buf) : "android-unknown";
}

#elif defined(__APPLE__)

static std::string PlatformDeviceId() {
    // macOS/iOS: 使用 sysctl hw.uuid (macOS) 或 hostname
    char buf[256] = {};
    size_t len = sizeof(buf);
    if (sysctlbyname("kern.uuid", buf, &len, nullptr, 0) == 0 && buf[0]) {
        return buf;
    }
    // 回退到 hostname
    gethostname(buf, sizeof(buf));
    return buf[0] ? std::string(buf) : "apple-unknown";
}

#elif defined(__linux__)

static std::string PlatformDeviceId() {
    // /etc/machine-id 是 systemd 稳定的机器标识
    std::ifstream ifs("/etc/machine-id");
    std::string id;
    if (ifs.is_open() && std::getline(ifs, id) && !id.empty()) {
        return id;
    }
    // 回退到 hostname
    char buf[256] = {};
    gethostname(buf, sizeof(buf));
    return buf[0] ? std::string(buf) : "linux-unknown";
}

#else

static std::string PlatformDeviceId() {
    return "unknown-device";
}

#endif

std::string GenerateDeviceId() {
    auto raw = PlatformDeviceId();

    // 用 hash 生成固定长度的 hex 字符串，避免暴露原始信息
    auto h = std::hash<std::string>{}(raw);
    std::ostringstream oss;
    oss << DetectDeviceType() << "-" << std::hex << h;
    return oss.str();
}

}  // namespace nova::client
