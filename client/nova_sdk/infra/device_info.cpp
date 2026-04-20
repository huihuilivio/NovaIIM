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

std::string DetectDeviceType() {
#if defined(__ANDROID__)
    return "mobile";
#elif defined(__APPLE__)
  #if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH
    return "mobile";
  #else
    return "pc";
  #endif
#elif defined(_WIN32) || defined(__linux__)
    return "pc";
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

/// FNV-1a 64-bit — 跨平台确定性哈希
static uint64_t Fnv1a64(const std::string& s) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (auto c : s) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

std::string GenerateDeviceId() {
    auto raw = PlatformDeviceId();

    // 用 FNV-1a 生成固定长度的 hex 字符串，避免暴露原始信息
    auto h = Fnv1a64(raw);
    std::ostringstream oss;
    oss << DetectDeviceType() << "-" << std::hex << h;
    return oss.str();
}

}  // namespace nova::client
