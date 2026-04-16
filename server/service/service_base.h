#pragma once

#include "../net/connection.h"
#include "../model/packet.h"
#include "../model/protocol.h"
#include "../core/server_context.h"

namespace nova {

// 服务基类：提供公共的 SendPacket 实现，消除三个 Service 中的重复代码
class ServiceBase {
protected:
    explicit ServiceBase(ServerContext& ctx) : ctx_(ctx) {}

    template <typename T>
    void SendPacket(const ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body) {
        Packet pkt;
        pkt.cmd = static_cast<uint16_t>(cmd);
        pkt.seq = seq;
        pkt.uid = uid;
        pkt.body = proto::Serialize(body);
        conn->Send(pkt);
        ctx_.incr_messages_out();
    }

    ServerContext& ctx_;
};

} // namespace nova
