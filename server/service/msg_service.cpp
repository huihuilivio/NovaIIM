#include "msg_service.h"
#include "../net/conn_manager.h"

namespace nova {

void MsgService::HandleSendMsg(ConnectionPtr conn, Packet& pkt) {
    (void)conn; (void)pkt;
    // TODO: 1. 解码 body → Message
    // TODO: 2. int64_t seq = GenerateSeq(conversation_id);
    // TODO: 3. 写 DB (dao)
    // TODO: 4. 返回 SEND_ACK 给发送方
    // TODO: 5. PushToUser(receiver_id, push_pkt)
    // TODO: 6. PushToOtherDevices(sender_id, conn->device_id(), push_pkt)
}

void MsgService::HandleDeliverAck(ConnectionPtr conn, Packet& pkt) {
    (void)conn; (void)pkt;
    // TODO: 标记消息已送达
}

void MsgService::HandleReadAck(ConnectionPtr conn, Packet& pkt) {
    (void)conn; (void)pkt;
    // TODO: 更新 conversation_members.last_read_seq
}

int64_t MsgService::GenerateSeq(int64_t conversation_id) {
    (void)conversation_id;
    // TODO: UPDATE conversations SET max_seq = max_seq + 1 WHERE id = ?
    // TODO: 后续优化 → 内存缓存 + CAS 自增
    return 0;
}

void MsgService::PushToUser(int64_t user_id, const Packet& pkt) {
    auto conns = ConnManager::Instance().GetConns(user_id);
    for (auto& c : conns) {
        c->Send(pkt);
    }
}

void MsgService::PushToOtherDevices(int64_t user_id, const std::string& exclude_device, const Packet& pkt) {
    auto conns = ConnManager::Instance().GetConns(user_id);
    for (auto& c : conns) {
        if (c->device_id() != exclude_device) {
            c->Send(pkt);
        }
    }
}

} // namespace nova
