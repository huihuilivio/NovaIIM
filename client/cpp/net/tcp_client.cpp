#include "tcp_client.h"
#include <core/logger.h>

#include <nova/packet.h>

namespace nova::client {

TcpClient::TcpClient(const ClientConfig& config)
    : config_(config) {}

TcpClient::~TcpClient() {
    Disconnect();
}

void TcpClient::Connect() {
    if (state_ != ConnectionState::kDisconnected &&
        state_ != ConnectionState::kReconnecting) {
        return;
    }

    SetState(ConnectionState::kConnecting);

    client_ = std::make_unique<hv::TcpClient>();
    int connfd = client_->createsocket(config_.server_port, config_.server_host.c_str());
    if (connfd < 0) {
        NOVA_LOG_ERROR("Failed to create socket to {}:{}", config_.server_host, config_.server_port);
        SetState(ConnectionState::kDisconnected);
        return;
    }

    // 按长度字段拆包（与服务端一致）
    unpack_setting_t setting{};
    setting.mode = UNPACK_BY_LENGTH_FIELD;
    setting.package_max_length = nova::proto::kHeaderSize + nova::proto::kMaxBodySize;
    setting.body_offset = 0;
    setting.length_field_offset = 14;   // cmd(2) + seq(4) + uid(8) = 14
    setting.length_field_bytes = 4;
    setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;
    setting.length_adjustment = nova::proto::kHeaderSize;
    client_->setUnpack(&setting);

    client_->onConnection = [this](const hv::SocketChannelPtr& channel) {
        if (channel->isConnected()) {
            NOVA_LOG_INFO("Connected to {}:{}", config_.server_host, config_.server_port);
            SetState(ConnectionState::kConnected);
            StartHeartbeat();
        } else {
            NOVA_LOG_WARN("Disconnected from server");
            StopHeartbeat();
            SetState(ConnectionState::kDisconnected);
        }
    };

    client_->onMessage = [this](const hv::SocketChannelPtr&, hv::Buffer* buf) {
        nova::proto::Packet pkt;
        if (nova::proto::Packet::Decode(
                reinterpret_cast<const char*>(buf->data()),
                buf->size(), pkt)) {
            if (on_packet_) {
                on_packet_(pkt);
            }
        } else {
            NOVA_LOG_WARN("Failed to decode packet ({} bytes)", buf->size());
        }
    };

    client_->start();
    NOVA_LOG_INFO("Connecting to {}:{}...", config_.server_host, config_.server_port);
}

void TcpClient::Disconnect() {
    StopHeartbeat();
    if (client_) {
        client_->stop();
        client_.reset();
    }
    SetState(ConnectionState::kDisconnected);
}

bool TcpClient::Send(const nova::proto::Packet& pkt) {
    auto data = pkt.Encode();
    if (data.empty()) return false;
    return SendRaw(data);
}

bool TcpClient::SendRaw(const std::string& data) {
    if (!client_) return false;
    auto state = state_.load();
    if (state != ConnectionState::kConnected &&
        state != ConnectionState::kAuthenticated) {
        return false;
    }
    std::lock_guard lock(send_mutex_);
    return client_->send(data) == static_cast<int>(data.size());
}

void TcpClient::SetState(ConnectionState s) {
    auto old = state_.exchange(s);
    if (old != s) {
        NOVA_LOG_DEBUG("State: {} -> {}", ConnectionStateStr(old), ConnectionStateStr(s));
        if (on_state_) {
            on_state_(s);
        }
    }
}

void TcpClient::StartHeartbeat() {
    if (!client_ || heartbeat_timer_) return;

    auto evloop = client_->loop();
    if (!evloop) return;
    hloop_t* raw_loop = evloop->loop();
    if (!raw_loop) return;

    heartbeat_timer_ = htimer_add(raw_loop, [](htimer_t* timer) {
        auto* self = static_cast<TcpClient*>(hevent_userdata(timer));
        if (self->state_ == ConnectionState::kAuthenticated) {
            nova::proto::Packet hb;
            hb.cmd = static_cast<uint16_t>(nova::proto::Cmd::kHeartbeat);
            hb.seq = self->NextSeq();
            self->Send(hb);
        }
    }, config_.heartbeat_interval_ms);

    hevent_set_userdata(heartbeat_timer_, this);
}

void TcpClient::StopHeartbeat() {
    if (heartbeat_timer_) {
        htimer_del(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

}  // namespace nova::client
