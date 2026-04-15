#include "user_service.h"
#include "../net/conn_manager.h"

namespace nova {

void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    (void)conn; (void)pkt;
    // TODO: 1. 解码 body → { uid, token }
    // TODO: 2. JWT 验证
    // TODO: 3. conn->set_user_id(...)
    // TODO: 4. ConnManager::Instance().Add(user_id, conn)
    // TODO: 5. 返回 LoginAck
}

void UserService::HandleLogout(ConnectionPtr conn, Packet& pkt) {
    (void)conn; (void)pkt;
    // TODO: 1. ConnManager::Instance().Remove(user_id, conn.get())
    // TODO: 2. conn->Close()
}

void UserService::HandleHeartbeat(ConnectionPtr conn, Packet& pkt) {
    // 回复心跳 ACK
    Packet ack;
    ack.cmd = static_cast<uint16_t>(Cmd::kHeartbeatAck);
    ack.uid = pkt.uid;
    conn->Send(ack);
}

} // namespace nova
