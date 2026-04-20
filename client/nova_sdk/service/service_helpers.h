#pragma once
// Service 内部辅助函数 — 仅供 Service .cpp 文件使用，不对外暴露

#include <core/client_context.h>
#include <service/result.h>

#include <nova/packet.h>
#include <nova/protocol.h>

namespace nova::client::detail {

template <typename Req>
nova::proto::Packet MakePacket(nova::proto::Cmd cmd, uint32_t seq, const Req& req) {
    nova::proto::Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(cmd);
    pkt.seq  = seq;
    pkt.body = nova::proto::Serialize(req);
    return pkt;
}

inline nova::proto::Packet MakePacket(nova::proto::Cmd cmd, uint32_t seq) {
    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(cmd);
    pkt.seq = seq;
    return pkt;
}

template <typename Ack, typename Cb>
void SendRequest(ClientContext& ctx, nova::proto::Packet& pkt,
                 Cb success_handler, std::function<void()> fail_cb) {
    ctx.Requests().AddPending(pkt.seq,
        [success_handler = std::move(success_handler)](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<Ack>(resp.body);
            success_handler(ack);
        },
        [fail_cb = std::move(fail_cb)](uint32_t) {
            if (fail_cb) fail_cb();
        }
    );
    ctx.SendPacket(pkt);
}

inline Result ToResult(const std::optional<nova::proto::RspBase>& ack) {
    if (!ack) return {.success = false, .msg = "deserialize error"};
    return {.success = (ack->code == 0), .msg = ack->msg};
}

}  // namespace nova::client::detail
