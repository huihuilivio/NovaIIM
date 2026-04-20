#include "client_context.h"
#include <infra/logger.h>

#include <nova/packet.h>
#include <nova/protocol.h>

namespace nova::client {

ClientContext::ClientContext(const ClientConfig& config)
    : config_(config) {}

ClientContext::~ClientContext() {
    Shutdown();
}

void ClientContext::Init() {
    nova::log::Init({
        .level = spdlog::level::from_str(config_.log_level),
        .file  = config_.log_file,
    });

    event_bus_.start();

    tcp_client_    = std::make_unique<TcpClient>();
    request_mgr_   = std::make_unique<RequestManager>(config_.request_timeout_ms);
    reconnect_mgr_ = std::make_unique<ReconnectManager>(config_);

    // 设置协议拆包规则
    // 包头: cmd(2) + seq(4) + uid(8) + body_len(4) = 18 bytes
    // length_field 在 offset=14 处，存的是 body_len
    tcp_client_->SetLengthFieldUnpack(
        nova::proto::kHeaderSize + nova::proto::kMaxBodySize,  // package_max_length
        nova::proto::kHeaderSize,   // body_offset (= head_len)
        14,                         // length_field_offset: cmd(2)+seq(4)+uid(8)
        4,                          // length_field_bytes
        0,                          // length_adjustment
        true                        // little_endian
    );

    // 网络层状态 → 业务层状态映射
    tcp_client_->OnStateChanged([this](ConnectionState s) {
        switch (s) {
            case ConnectionState::kConnecting:
                SetState(ClientState::kConnecting);
                break;
            case ConnectionState::kConnected:
                SetState(ClientState::kConnected);
                StartHeartbeat();
                break;
            case ConnectionState::kDisconnected:
                StopHeartbeat();
                uid_.clear();
                // 如果是从已连接/已认证断开，且重连已启用，则进入重连状态
                if (reconnect_mgr_->IsEnabled() &&
                    state_.load() != ClientState::kDisconnected) {
                    SetState(ClientState::kReconnecting);
                } else {
                    SetState(ClientState::kDisconnected);
                }
                break;
        }
        reconnect_mgr_->OnStateChanged(state_.load());
    });

    // 重连回调
    reconnect_mgr_->OnReconnect([this]() {
        tcp_client_->Connect(config_.server_host, config_.server_port);
    });

    SetupPacketDispatch();
    StartTimeoutChecker();

    NOVA_LOG_INFO("ClientContext initialized (server={}:{})",
                  config_.server_host, config_.server_port);
}

void ClientContext::Shutdown() {
    StopHeartbeat();
    StopTimeoutChecker();
    if (reconnect_mgr_) reconnect_mgr_->Stop();
    if (tcp_client_) tcp_client_->Disconnect();
    if (request_mgr_) request_mgr_->CancelAll();
    event_bus_.stop();
    uid_.clear();
    SetState(ClientState::kDisconnected);
    NOVA_LOG_INFO("ClientContext shutdown");
}

void ClientContext::Connect() {
    tcp_client_->Connect(config_.server_host, config_.server_port);
}

void ClientContext::SetState(ClientState s) {
    auto old = state_.exchange(s);
    if (old != s) {
        NOVA_LOG_INFO("ClientState: {} → {}", ClientStateStr(old), ClientStateStr(s));
        std::vector<StateCallback> snapshot;
        {
            std::lock_guard lock(state_cb_mutex_);
            snapshot = state_callbacks_;
        }
        for (auto& cb : snapshot) cb(s);
    }
}

void ClientContext::SetAuthenticated(const std::string& uid) {
    uid_ = uid;
    SetState(ClientState::kAuthenticated);
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
    if (heartbeat_timer_id_) return;

    heartbeat_timer_id_ = timer_.SetInterval(config_.heartbeat_interval_ms, [this](Timer::TimerID) {
        if (tcp_client_->GetState() == ConnectionState::kConnected && IsLoggedIn()) {
            nova::proto::Packet hb;
            hb.cmd = static_cast<uint16_t>(nova::proto::Cmd::kHeartbeat);
            hb.seq = NextSeq();
            SendPacket(hb);
        }
    });
}

void ClientContext::StopHeartbeat() {
    if (heartbeat_timer_id_) {
        timer_.KillTimer(heartbeat_timer_id_);
        heartbeat_timer_id_ = 0;
    }
}

void ClientContext::StartTimeoutChecker() {
    if (timeout_checker_id_) return;

    // 每秒检查一次请求超时
    timeout_checker_id_ = timer_.SetInterval(1000, [this](Timer::TimerID) {
        if (request_mgr_) {
            request_mgr_->CheckTimeouts();
        }
    });
}

void ClientContext::StopTimeoutChecker() {
    if (timeout_checker_id_) {
        timer_.KillTimer(timeout_checker_id_);
        timeout_checker_id_ = 0;
    }
}

}  // namespace nova::client
