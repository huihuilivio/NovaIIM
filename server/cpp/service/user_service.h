#pragma once

#include "../net/connection.h"
#include "../model/packet.h"

namespace nova {

// 用户服务
// 职责：登录认证（JWT）、登出
class UserService {
public:
    UserService() = default;

    // 处理登录
    void HandleLogin(ConnectionPtr conn, Packet& pkt);

    // 处理登出
    void HandleLogout(ConnectionPtr conn, Packet& pkt);

    // 处理心跳
    void HandleHeartbeat(ConnectionPtr conn, Packet& pkt);
};

} // namespace nova
