#pragma once

#include "service_base.h"

namespace nova {

// 同步服务（对应架构文档 4.5 SyncService）
// 职责：离线拉取、多端同步、未读管理
class SyncService : public ServiceBase {
public:
    explicit SyncService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleSyncMsg(ConnectionPtr conn, Packet& pkt);
    void HandleSyncUnread(ConnectionPtr conn, Packet& pkt);
};

} // namespace nova
