#pragma once

#include "../net/connection.h"
#include "../model/packet.h"
#include "../model/protocol.h"

namespace nova {

class ServerContext;

// 消息服务（对应架构文档 4.4 MsgService）
// 职责：seq 生成、写 DB、推送消息、ACK 处理
class MsgService {
public:
    explicit MsgService(ServerContext& ctx) : ctx_(ctx) {}

    void HandleSendMsg(ConnectionPtr conn, Packet& pkt);
    void HandleDeliverAck(ConnectionPtr conn, Packet& pkt);
    void HandleReadAck(ConnectionPtr conn, Packet& pkt);

private:
    int64_t GenerateSeq(int64_t conversation_id);
    void PushToUser(int64_t user_id, const Packet& pkt);
    void PushToOtherDevices(int64_t user_id, const std::string& exclude_device, const Packet& pkt);

    template <typename T>
    void SendPacket(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body);

    ServerContext& ctx_;
};

} // namespace nova
