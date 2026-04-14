#include "router.h"
// #include <spdlog/spdlog.h>

namespace nova {

void Router::Dispatch(ConnectionPtr conn, Packet& pkt) {
    auto cmd = static_cast<Cmd>(pkt.cmd);
    auto it = handlers_.find(cmd);
    if (it != handlers_.end()) {
        it->second(std::move(conn), pkt);
    } else {
        // TODO: spdlog::warn("Unknown cmd: 0x{:04X}", pkt.cmd);
    }
}

} // namespace nova
