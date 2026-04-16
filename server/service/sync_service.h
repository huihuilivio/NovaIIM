#pragma once

#include "../net/connection.h"
#include "../model/packet.h"
#include "../model/protocol.h"

namespace nova {

class ServerContext;

// 同步服务（对应架构文档 4.5 SyncService）
// 职责：离线拉取、多端同步、未读管理
class SyncService {
public:
    explicit SyncService(ServerContext& ctx) : ctx_(ctx) {}

    void HandleSyncMsg(ConnectionPtr conn, Packet& pkt);
    void HandleSyncUnread(ConnectionPtr conn, Packet& pkt);

private:
    template <typename T>
    void SendPacket(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body);

    ServerContext& ctx_;
};

} // namespace nova
