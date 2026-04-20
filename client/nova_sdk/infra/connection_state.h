#pragma once
// ConnectionState — 连接状态机

#include <cstdint>

namespace nova::client {

enum class ConnectionState : uint8_t {
    kDisconnected = 0,   // 未连接
    kConnecting   = 1,   // 连接中
    kConnected    = 2,   // 已连接
};

inline const char* ConnectionStateStr(ConnectionState s) {
    switch (s) {
        case ConnectionState::kDisconnected:  return "Disconnected";
        case ConnectionState::kConnecting:    return "Connecting";
        case ConnectionState::kConnected:     return "Connected";
        default: return "Unknown";
    }
}

}  // namespace nova::client
