#include "tcp_client.h"

#include <hv/TcpClient.h>
#include <hv/hloop.h>

#include <spdlog/spdlog.h>

#include <atomic>
#include <mutex>

namespace nova::client {

struct TcpClient::Impl {
    std::unique_ptr<hv::TcpClient> client;
    unpack_setting_t unpack{};
    bool has_unpack = false;

    std::atomic<ConnectionState> state{ConnectionState::kDisconnected};

    DataCallback  on_data;
    StateCallback on_state;

    std::mutex send_mutex;

    void SetState(ConnectionState s) {
        auto old = state.exchange(s);
        if (old != s && on_state) {
            on_state(s);
        }
    }
};

TcpClient::TcpClient() : impl_(std::make_unique<Impl>()) {}

TcpClient::~TcpClient() {
    Disconnect();
}

void TcpClient::SetLengthFieldUnpack(uint32_t package_max_length,
                                     uint16_t body_offset,
                                     uint16_t length_field_offset,
                                     uint16_t length_field_bytes,
                                     int16_t  length_adjustment,
                                     bool     little_endian) {
    auto& s = impl_->unpack;
    s.mode = UNPACK_BY_LENGTH_FIELD;
    s.package_max_length = package_max_length;
    s.body_offset = body_offset;
    s.length_field_offset = length_field_offset;
    s.length_field_bytes = length_field_bytes;
    s.length_adjustment = length_adjustment;
    s.length_field_coding = little_endian ? ENCODE_BY_LITTEL_ENDIAN : ENCODE_BY_BIG_ENDIAN;
    impl_->has_unpack = true;
}

void TcpClient::Connect(const std::string& host, uint16_t port) {
    if (impl_->state != ConnectionState::kDisconnected) {
        return;
    }

    impl_->SetState(ConnectionState::kConnecting);

    impl_->client = std::make_unique<hv::TcpClient>();
    int connfd = impl_->client->createsocket(port, host.c_str());
    if (connfd < 0) {
        spdlog::error("[TcpClient] Failed to create socket to {}:{}", host, port);
        impl_->SetState(ConnectionState::kDisconnected);
        return;
    }

    if (impl_->has_unpack) {
        impl_->client->setUnpack(&impl_->unpack);
    }

    impl_->client->onConnection = [this, host, port](const hv::SocketChannelPtr& channel) {
        if (channel->isConnected()) {
            spdlog::info("[TcpClient] Connected to {}:{}", host, port);
            impl_->SetState(ConnectionState::kConnected);
        } else {
            spdlog::warn("[TcpClient] Disconnected from server");
            impl_->SetState(ConnectionState::kDisconnected);
        }
    };

    impl_->client->onMessage = [this](const hv::SocketChannelPtr&, hv::Buffer* buf) {
        if (!buf || buf->size() == 0) return;
        if (impl_->on_data) {
            impl_->on_data(buf->data(), buf->size());
        }
    };

    impl_->client->start();
    spdlog::info("[TcpClient] Connecting to {}:{}...", host, port);
}

void TcpClient::Disconnect() {
    {
        std::lock_guard lock(impl_->send_mutex);
        if (impl_->client) {
            impl_->client->stop();
            impl_->client.reset();
        }
    }
    impl_->SetState(ConnectionState::kDisconnected);
}

bool TcpClient::Send(const void* data, size_t len) {
    std::lock_guard lock(impl_->send_mutex);
    if (!impl_->client) return false;
    auto state = impl_->state.load();
    if (state != ConnectionState::kConnected) {
        return false;
    }
    return impl_->client->send(data, len) == static_cast<int>(len);
}

bool TcpClient::Send(const std::string& data) {
    return Send(data.data(), data.size());
}

ConnectionState TcpClient::GetState() const {
    return impl_->state.load();
}

void TcpClient::OnData(DataCallback cb) {
    impl_->on_data = std::move(cb);
}

void TcpClient::OnStateChanged(StateCallback cb) {
    impl_->on_state = std::move(cb);
}

}  // namespace nova::client
