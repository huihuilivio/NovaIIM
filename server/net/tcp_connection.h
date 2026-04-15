#pragma once

#include "connection.h"

#include <hv/Channel.h>

namespace nova {

// TCP 连接实现 —— 持有 libhv SocketChannel
class TcpConnection : public Connection {
public:
    explicit TcpConnection(const hv::SocketChannelPtr& channel)
        : channel_(channel) {}

    void Send(const Packet& pkt) override {
        std::string frame = pkt.Encode();
        channel_->write(frame);
    }

    void Close() override {
        channel_->close();
    }

    hv::SocketChannelPtr channel() const { return channel_; }

private:
    hv::SocketChannelPtr channel_;
};

} // namespace nova
