#pragma once

#include "service_base.h"
#include <nova/protocol.h>

namespace nova {

// ConvUpdate 推送辅助（跨服务调用）
void BroadcastConvUpdate(ServerContext& ctx, int64_t conversation_id,
                         int64_t exclude_user_id, const proto::ConvUpdateMsg& update);

// 会话服务：会话列表、隐藏/恢复、免打扰、置顶
class ConvService : public ServiceBase {
public:
    explicit ConvService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleGetConvList(ConnectionPtr conn, Packet& pkt);
    void HandleDeleteConv(ConnectionPtr conn, Packet& pkt);
    void HandleMuteConv(ConnectionPtr conn, Packet& pkt);
    void HandlePinConv(ConnectionPtr conn, Packet& pkt);
};

}  // namespace nova
