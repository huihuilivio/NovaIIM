#include "router.h"
#include "../core/logger.h"
#include <nova/errors.h>
#include <nova/protocol.h>

namespace nova {

static constexpr const char* kLogTag = "Router";

void Router::Dispatch(ConnectionPtr conn, Packet& pkt) {
    auto cmd = static_cast<Cmd>(pkt.cmd);

    // 认证守卫：未登录连接只允许发送 Login / Register 命令
    if (cmd != Cmd::kLogin && cmd != Cmd::kRegister && !conn->is_authenticated()) {
        NOVA_NLOG_WARN(kLogTag, "unauthenticated request cmd=0x{:04X}, dropping", pkt.cmd);
        Packet err;
        err.cmd  = pkt.cmd;
        err.seq  = pkt.seq;
        err.uid  = pkt.uid;
        err.body = proto::Serialize(proto::RspBase{errc::kNotAuthenticated.code, errc::kNotAuthenticated.msg});
        conn->Send(err);
        return;
    }

    auto it = handlers_.find(cmd);
    if (it != handlers_.end()) {
        it->second(std::move(conn), pkt);
    } else {
        NOVA_NLOG_WARN(kLogTag, "unknown cmd=0x{:04X} from uid={}", pkt.cmd, conn->uid());
        Packet err;
        err.cmd  = pkt.cmd;
        err.seq  = pkt.seq;
        err.uid  = pkt.uid;
        err.body = proto::Serialize(proto::RspBase{errc::kInvalidBody.code, "unknown command"});
        conn->Send(err);
    }
}

}  // namespace nova
