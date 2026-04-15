#pragma once

#include "../net/connection.h"
#include "../model/packet.h"

namespace nova {

class ServerContext;

// 用户服务
// 职责：登录认证（密码校验）、登出、心跳
class UserService {
public:
    explicit UserService(ServerContext& ctx) : ctx_(ctx) {}

    // 处理登录
    void HandleLogin(ConnectionPtr conn, Packet& pkt);

    // 处理登出
    void HandleLogout(ConnectionPtr conn, Packet& pkt);

    // 处理心跳
    void HandleHeartbeat(ConnectionPtr conn, Packet& pkt);

private:
    // 发送应答包
    void SendReply(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const std::string& body);

    ServerContext& ctx_;
};

} // namespace nova
