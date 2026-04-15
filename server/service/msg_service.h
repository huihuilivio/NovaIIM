#pragma once

#include "../net/connection.h"
#include "../model/packet.h"

namespace nova {

class ServerContext;

// 消息服务（对应架构文档 4.4 MsgService）
// 职责：seq 生成、写 DB、推送消息、ACK 处理
class MsgService {
public:
    explicit MsgService(ServerContext& ctx) : ctx_(ctx) {}

    // 处理发送消息请求
    // 流程：生成 seq → 写 DB → 返回 SEND_ACK → 推送给接收方 → 推送给发送方其他端
    void HandleSendMsg(ConnectionPtr conn, Packet& pkt);

    // 处理投递确认
    void HandleDeliverAck(ConnectionPtr conn, Packet& pkt);

    // 处理已读确认 → 更新 read_cursor
    void HandleReadAck(ConnectionPtr conn, Packet& pkt);

private:
    // 生成 seq（对应架构文档第 6 节）
    // seq = conversation.max_seq + 1 (原子递增)
    int64_t GenerateSeq(int64_t conversation_id);

    // 推送消息给目标用户的所有端
    void PushToUser(int64_t user_id, const Packet& pkt);

    // 推送给发送者的其他端（多端同步）
    void PushToOtherDevices(int64_t user_id, const std::string& exclude_device, const Packet& pkt);

    // 发送应答包
    void SendReply(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const std::string& body);

    ServerContext& ctx_;
};

} // namespace nova
