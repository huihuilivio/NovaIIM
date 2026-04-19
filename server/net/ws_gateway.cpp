#include "ws_gateway.h"
#include "../core/server_context.h"
#include "../core/logger.h"

namespace nova {

static constexpr const char* kLogTag = "WsGateway";

WsGateway::WsGateway(ServerContext& ctx) : ctx_(ctx) {}

WsGateway::~WsGateway() {
    Stop();
}

int WsGateway::Start(int port) {
    if (server_) {
        NOVA_NLOG_WARN(kLogTag, "already started, call Stop() first");
        return -1;
    }

    // ---- WebSocket 回调 ----

    ws_service_.onopen = [this](const WebSocketChannelPtr& channel, const HttpRequestPtr& /*req*/) {
        ctx_.add_connection();
        auto conn = std::make_shared<WsConnection>(channel);
        channel->setContextPtr(conn);
        NOVA_NLOG_DEBUG(kLogTag, "ws connected: {}", channel->peeraddr());
    };

    ws_service_.onmessage = [this](const WebSocketChannelPtr& channel, const std::string& msg) {
        auto conn = channel->getContextPtr<WsConnection>();
        if (!conn) {
            channel->close();
            return;
        }

        Packet pkt;
        if (!Packet::Decode(msg.data(), msg.size(), pkt)) {
            NOVA_NLOG_WARN(kLogTag, "invalid packet from {}, closing", channel->peeraddr());
            ctx_.incr_bad_packets();
            channel->close();
            return;
        }

        if (handler_) {
            ctx_.incr_messages_in();
            handler_(conn, pkt);
        }
    };

    ws_service_.onclose = [this](const WebSocketChannelPtr& channel) {
        auto conn = channel->getContextPtr<WsConnection>();
        if (conn && conn->is_authenticated()) {
            ctx_.conn_manager().Remove(conn->user_id(), conn.get());
        }
        ctx_.remove_connection();
        NOVA_NLOG_DEBUG(kLogTag, "ws closed: {}", channel->peeraddr());
    };

    ws_service_.ping_interval = ping_interval_ms_;

    // ---- 启动 WebSocketServer ----

    server_ = std::make_unique<hv::WebSocketServer>(&ws_service_);
    server_->setPort(port);

    int ret = server_->start();
    if (ret != 0) {
        NOVA_NLOG_ERROR(kLogTag, "failed to start on port {} (ret={})", port, ret);
        server_.reset();
        return -1;
    }

    NOVA_NLOG_INFO(kLogTag, "WebSocket server started on port {}", port);
    return 0;
}

void WsGateway::Stop() {
    if (server_) {
        server_->stop();
        server_.reset();
        NOVA_NLOG_INFO(kLogTag, "stopped");
    }
}

}  // namespace nova
