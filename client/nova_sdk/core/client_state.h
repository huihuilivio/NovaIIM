#pragma once
// ClientState — 业务层客户端状态

#include <cstdint>

namespace nova::client {

enum class ClientState : uint8_t {
    kDisconnected  = 0,   // 未连接
    kConnecting    = 1,   // 连接中
    kConnected     = 2,   // 已连接（未认证）
    kAuthenticated = 3,   // 已认证（可发业务消息）
    kReconnecting  = 4,   // 断线重连中
};

inline const char* ClientStateStr(ClientState s) {
    switch (s) {
        case ClientState::kDisconnected:   return "Disconnected";
        case ClientState::kConnecting:     return "Connecting";
        case ClientState::kConnected:      return "Connected";
        case ClientState::kAuthenticated:  return "Authenticated";
        case ClientState::kReconnecting:   return "Reconnecting";
        default: return "Unknown";
    }
}

}  // namespace nova::client
