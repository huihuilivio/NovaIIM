#pragma once

#include "../net/connection.h"
#include "../model/packet.h"
#include "../model/protocol.h"

namespace nova {

class ServerContext;

// 用户服务
// 职责：登录认证（密码校验）、登出、心跳
class UserService {
public:
    explicit UserService(ServerContext& ctx) : ctx_(ctx) {}

    void HandleLogin(ConnectionPtr conn, Packet& pkt);
    void HandleLogout(ConnectionPtr conn, Packet& pkt);
    void HandleHeartbeat(ConnectionPtr conn, Packet& pkt);

private:
    template <typename T>
    void SendPacket(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body);

    ServerContext& ctx_;
};

} // namespace nova
