#include "client_context.h"
#include "logger.h"

#include <nova/packet.h>
#include <nova/protocol.h>

namespace nova::client {

ClientContext::ClientContext(const ClientConfig& config)
    : config_(config) {}

ClientContext::~ClientContext() {
    Shutdown();
}

void ClientContext::Init() {
    Logger::Init("nova_client", config_.log_file, config_.log_level);

    tcp_client_    = std::make_unique<TcpClient>(config_);
    request_mgr_   = std::make_unique<RequestManager>(config_.request_timeout_ms);
    reconnect_mgr_ = std::make_unique<ReconnectManager>(config_);

    // 连接状态 → 重连管理器
    tcp_client_->OnStateChanged([this](ConnectionState s) {
        reconnect_mgr_->OnStateChanged(s);
    });

    // 重连回调
    reconnect_mgr_->OnReconnect([this]() {
        tcp_client_->Connect();
    });

    SetupPacketDispatch();

    NOVA_LOG_INFO("ClientContext initialized (server={}:{})",
                  config_.server_host, config_.server_port);
}

void ClientContext::Shutdown() {
    if (reconnect_mgr_) reconnect_mgr_->Stop();
    if (tcp_client_) tcp_client_->Disconnect();
    if (request_mgr_) request_mgr_->CancelAll();
    uid_.clear();
    NOVA_LOG_INFO("ClientContext shutdown");
}

void ClientContext::SetupPacketDispatch() {
    tcp_client_->OnPacket([this](const nova::proto::Packet& pkt) {
        // 先尝试 request-response 匹配
        if (request_mgr_->HandleResponse(pkt)) {
            return;
        }

        // 服务端主推消息 → EventBus 分发
        using Cmd = nova::proto::Cmd;
        auto cmd = static_cast<Cmd>(pkt.cmd);

        switch (cmd) {
            case Cmd::kPushMsg: {
                auto msg = nova::proto::Deserialize<nova::proto::PushMsg>(pkt.body);
                if (msg) Events().Publish(*msg);
                break;
            }
            case Cmd::kRecallNotify: {
                auto notify = nova::proto::Deserialize<nova::proto::RecallNotify>(pkt.body);
                if (notify) Events().Publish(*notify);
                break;
            }
            case Cmd::kFriendNotify: {
                auto notify = nova::proto::Deserialize<nova::proto::FriendNotifyMsg>(pkt.body);
                if (notify) Events().Publish(*notify);
                break;
            }
            case Cmd::kConvUpdate: {
                auto update = nova::proto::Deserialize<nova::proto::ConvUpdateMsg>(pkt.body);
                if (update) Events().Publish(*update);
                break;
            }
            case Cmd::kGroupNotify: {
                auto notify = nova::proto::Deserialize<nova::proto::GroupNotifyMsg>(pkt.body);
                if (notify) Events().Publish(*notify);
                break;
            }
            case Cmd::kHeartbeatAck:
                // 心跳应答，无需处理
                break;
            default:
                NOVA_LOG_DEBUG("Unhandled push cmd=0x{:04x} seq={}", pkt.cmd, pkt.seq);
                break;
        }
    });
}

}  // namespace nova::client
