#pragma once

#include "connection.h"

#include <hv/Channel.h>

namespace nova {

// TCP 连接实现 —— 持有 libhv SocketChannel
// 注意: hv::Channel::write() 是线程安全的（内部跨线程投递到 IO 线程），
// 因此多个 Worker 线程并发调用 Send() 不会产生数据竞争。
class TcpConnection : public Connection {
public:
    explicit TcpConnection(const hv::SocketChannelPtr& channel) : channel_(channel) {}

    void Send(const Packet& pkt) override {
        std::string frame = pkt.Encode();
        channel_->write(frame);
    }

    void SendEncoded(const std::string& data) override { channel_->write(data); }

    void Close() override { channel_->close(); }

    hv::SocketChannelPtr channel() const { return channel_; }

private:
    hv::SocketChannelPtr channel_;
};

}  // namespace nova
