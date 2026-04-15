#pragma once

#include "../net/connection.h"
#include "../model/packet.h"

namespace nova {

class ServerContext;

// 同步服务（对应架构文档 4.5 SyncService）
// 职责：离线拉取、多端同步、未读管理
class SyncService {
public:
    explicit SyncService(ServerContext& ctx) : ctx_(ctx) {}

    // 处理离线消息同步请求
    // 拉取 msg.seq > last_read_seq 的消息
    void HandleSyncMsg(ConnectionPtr conn, Packet& pkt);

    // 处理未读计数同步请求
    void HandleSyncUnread(ConnectionPtr conn, Packet& pkt);

private:
    void SendReply(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const std::string& body);

    ServerContext& ctx_;
};

} // namespace nova
