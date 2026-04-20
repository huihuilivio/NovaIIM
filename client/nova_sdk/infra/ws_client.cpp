#include "ws_client.h"

#include <hv/WebSocketClient.h>

#include <spdlog/spdlog.h>

#include <atomic>
#include <map>
#include <mutex>

namespace nova::client {

struct WsClient::Impl {
    std::unique_ptr<hv::WebSocketClient> client;
    std::map<std::string, std::string> headers;
    std::atomic<ConnectionState> state{ConnectionState::kDisconnected};

    MessageCallback on_message;
    BinaryCallback  on_binary;
    StateCallback   on_state;

    std::mutex send_mutex;

    void SetState(ConnectionState s) {
        auto old = state.exchange(s);
        if (old != s && on_state) {
            on_state(s);
        }
    }
};

WsClient::WsClient() : impl_(std::make_unique<Impl>()) {}

WsClient::~WsClient() {
    Disconnect();
}

void WsClient::SetHeader(const std::string& key, const std::string& value) {
    impl_->headers[key] = value;
}

void WsClient::Connect(const std::string& url) {
    if (impl_->state != ConnectionState::kDisconnected) return;

    impl_->SetState(ConnectionState::kConnecting);

    impl_->client = std::make_unique<hv::WebSocketClient>();

    impl_->client->onopen = [this]() {
        spdlog::info("[WsClient] Connected");
        impl_->SetState(ConnectionState::kConnected);
    };

    impl_->client->onclose = [this]() {
        spdlog::warn("[WsClient] Disconnected");
        impl_->SetState(ConnectionState::kDisconnected);
    };

    impl_->client->onmessage = [this](const std::string& msg) {
        if (impl_->on_message) impl_->on_message(msg);
    };

    // 构造请求头并连接
    http_headers hdr;
    for (auto& [k, v] : impl_->headers) {
        hdr[k] = v;
    }
    impl_->client->open(url.c_str(), hdr);
    spdlog::info("[WsClient] Connecting to {}...", url);
}

void WsClient::Disconnect() {
    {
        std::lock_guard lock(impl_->send_mutex);
        if (impl_->client) {
            impl_->client->close();
            impl_->client.reset();
        }
    }
    impl_->SetState(ConnectionState::kDisconnected);
}

bool WsClient::Send(const std::string& msg) {
    std::lock_guard lock(impl_->send_mutex);
    if (!impl_->client || impl_->state != ConnectionState::kConnected) return false;
    return impl_->client->send(msg) == static_cast<int>(msg.size());
}

bool WsClient::SendBinary(const void* data, size_t len) {
    std::lock_guard lock(impl_->send_mutex);
    if (!impl_->client || impl_->state != ConnectionState::kConnected) return false;
    return impl_->client->send(static_cast<const char*>(data), len,
                               WS_OPCODE_BINARY) == static_cast<int>(len);
}

ConnectionState WsClient::GetState() const {
    return impl_->state.load();
}

void WsClient::OnMessage(MessageCallback cb) {
    impl_->on_message = std::move(cb);
}

void WsClient::OnBinary(BinaryCallback cb) {
    impl_->on_binary = std::move(cb);
}

void WsClient::OnStateChanged(StateCallback cb) {
    impl_->on_state = std::move(cb);
}

}  // namespace nova::client
