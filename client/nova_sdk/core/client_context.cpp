#include "client_context.h"
#include "logger.h"

#include <nova/packet.h>
#include <nova/protocol.h>

#include <hv/hloop.h>

namespace nova::client {

ClientContext::ClientContext(const ClientConfig& config)
    : config_(config) {}

ClientContext::~ClientContext() {
    Shutdown();
}

void ClientContext::Init() {
    Logger::Init("nova_sdk", config_.log_file, config_.log_level);

    event_bus_.start();

    tcp_client_    = std::make_unique<TcpClient>();
    request_mgr_   = std::make_unique<RequestManager>(config_.request_timeout_ms);
    reconnect_mgr_ = std::make_unique<ReconnectManager>(config_);

    // 设置协议拆包规则
    // 包头: cmd(2) + seq(4) + uid(8) + body_len(4) = 18 bytes
    // length_field 在 offset=14 处，存的是 body_len，需要加 header 还原总长
    tcp_client_->SetLengthFieldUnpack(
        nova::proto::kHeaderSize + nova::proto::kMaxBodySize,  // package_max_length
        0,                          // body_offset
        14,                         // length_field_offset: cmd(2)+seq(4)+uid(8)
        4,                          // length_field_bytes
        nova::proto::kHeaderSize,   // length_adjustment
        true                        // little_endian
    );

    // 连接状态 → 重连管理器 + 心跳管理
    tcp_client_->OnStateChanged([this](ConnectionState s) {
        reconnect_mgr_->OnStateChanged(s);
        if (s == ConnectionState::kConnected) {
            StartHeartbeat();
        } else if (s == ConnectionState::kDisconnected) {
            StopHeartbeat();
        }
    });

    // 重连回调
    reconnect_mgr_->OnReconnect([this]() {
        tcp_client_->Connect(config_.server_host, config_.server_port);
    });

    SetupPacketDispatch();

    NOVA_LOG_INFO("ClientContext initialized (server={}:{})",
                  config_.server_host, config_.server_port);
}

void ClientContext::Shutdown() {
    StopHeartbeat();
    if (reconnect_mgr_) reconnect_mgr_->Stop();
    if (tcp_client_) tcp_client_->Disconnect();
    if (request_mgr_) request_mgr_->CancelAll();
    event_bus_.stop();
    uid_.clear();
    NOVA_LOG_INFO("ClientContext shutdown");
}

void ClientContext::Connect() {
    tcp_client_->Connect(config_.server_host, config_.server_port);
}

bool ClientContext::SendPacket(const nova::proto::Packet& pkt) {
    auto data = pkt.Encode();
    if (data.empty()) return false;
    return tcp_client_->Send(data);
}

void ClientContext::SetupPacketDispatch() {
    tcp_client_->OnData([this](const void* data, size_t len) {
        nova::proto::Packet pkt;
        if (!nova::proto::Packet::Decode(
                reinterpret_cast<const char*>(data), len, pkt)) {
            NOVA_LOG_WARN("Failed to decode packet ({} bytes)", len);
            return;
        }

        // 先尝试 request-response 匹配
        if (request_mgr_->HandleResponse(pkt)) {
            return;
        }

        // 服务端主推消息 → MessageBus 分发
        using Cmd = nova::proto::Cmd;
        auto cmd = static_cast<Cmd>(pkt.cmd);

        switch (cmd) {
            case Cmd::kPushMsg: {
                auto msg = nova::proto::Deserialize<nova::proto::PushMsg>(pkt.body);
                if (msg) Events().publish<nova::proto::PushMsg>("PushMsg", std::move(*msg));
                break;
            }
            case Cmd::kRecallNotify: {
                auto notify = nova::proto::Deserialize<nova::proto::RecallNotify>(pkt.body);
                if (notify) Events().publish<nova::proto::RecallNotify>("RecallNotify", std::move(*notify));
                break;
            }
            case Cmd::kFriendNotify: {
                auto notify = nova::proto::Deserialize<nova::proto::FriendNotifyMsg>(pkt.body);
                if (notify) Events().publish<nova::proto::FriendNotifyMsg>("FriendNotify", std::move(*notify));
                break;
            }
            case Cmd::kConvUpdate: {
                auto update = nova::proto::Deserialize<nova::proto::ConvUpdateMsg>(pkt.body);
                if (update) Events().publish<nova::proto::ConvUpdateMsg>("ConvUpdate", std::move(*update));
                break;
            }
            case Cmd::kGroupNotify: {
                auto notify = nova::proto::Deserialize<nova::proto::GroupNotifyMsg>(pkt.body);
                if (notify) Events().publish<nova::proto::GroupNotifyMsg>("GroupNotify", std::move(*notify));
                break;
            }
            case Cmd::kHeartbeatAck:
                break;
            default:
                NOVA_LOG_DEBUG("Unhandled push cmd=0x{:04x} seq={}", pkt.cmd, pkt.seq);
                break;
        }
    });
}

void ClientContext::StartHeartbeat() {
    if (heartbeat_timer_) return;
    hloop_t* raw_loop = tcp_client_->GetRawLoop();
    if (!raw_loop) return;

    heartbeat_timer_ = htimer_add(raw_loop, [](htimer_t* timer) {
        auto* self = static_cast<ClientContext*>(hevent_userdata(timer));
        if (!self) return;
        if (self->tcp_client_->GetState() == ConnectionState::kAuthenticated) {
            nova::proto::Packet hb;
            hb.cmd = static_cast<uint16_t>(nova::proto::Cmd::kHeartbeat);
            hb.seq = self->NextSeq();
            self->SendPacket(hb);
        }
    }, config_.heartbeat_interval_ms);

    hevent_set_userdata(heartbeat_timer_, this);
}

void ClientContext::StopHeartbeat() {
    if (heartbeat_timer_) {
        htimer_del(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

}  // namespace nova::client
