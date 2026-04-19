#pragma once

#include "connection.h"

#include <hv/WebSocketChannel.h>

namespace nova {

// WebSocket 连接实现 —— 持有 libhv WebSocketChannel
// 使用二进制帧格式，与 TcpConnection 帧格式一致。
//
// hv::WebSocketChannel::send() 内部加锁，多线程并发调用安全。
class WsConnection : public Connection {
public:
    explicit WsConnection(const WebSocketChannelPtr& channel) : channel_(channel) {}

    void Send(const Packet& pkt) override {
        std::string frame = pkt.Encode();
        channel_->send(frame.data(), static_cast<int>(frame.size()), WS_OPCODE_BINARY);
    }

    void SendEncoded(const std::string& data) override {
        channel_->send(data.data(), static_cast<int>(data.size()), WS_OPCODE_BINARY);
    }

    void Close() override { channel_->close(); }

    WebSocketChannelPtr channel() const { return channel_; }

private:
    WebSocketChannelPtr channel_;
};

}  // namespace nova
