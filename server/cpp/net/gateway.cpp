#include "gateway.h"
#include "conn_manager.h"
#include "../core/logger.h"

namespace nova {

static constexpr const char* kLogTag = "Gateway";

Gateway::~Gateway() {
    Stop();
}

int Gateway::Start(int port) {
    server_ = std::make_unique<hv::TcpServer>();

    // 配置 UNPACK_BY_LENGTH_FIELD —— 自动拆包
    // 帧: cmd(2) + seq(4) + uid(8) + body_len(4) + body(N)
    // body_len 位于偏移 14，占 4 字节，小端序，存储 body 长度
    unpack_setting_.mode                = UNPACK_BY_LENGTH_FIELD;
    unpack_setting_.package_max_length  = kHeaderSize + kMaxBodySize;
    unpack_setting_.body_offset         = static_cast<unsigned short>(kHeaderSize);
    unpack_setting_.length_field_offset = 14;   // cmd(2)+seq(4)+uid(8) = 14
    unpack_setting_.length_field_bytes  = 4;
    unpack_setting_.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;
    unpack_setting_.length_adjustment   = 0;
    server_->setUnpack(&unpack_setting_);

    server_->setThreadNum(worker_threads_);

    int listenfd = server_->createsocket(port);
    if (listenfd < 0) {
        NOVA_NLOG_ERROR(kLogTag, "createsocket on port {} failed (fd={})", port, listenfd);
        return -1;
    }

    server_->onConnection = [this](const hv::SocketChannelPtr& channel) {
        OnConnection(channel);
    };
    server_->onMessage = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
        OnMessage(channel, buf);
    };

    server_->start();
    NOVA_NLOG_INFO(kLogTag, "TCP server started on port {} (workers={})", port, worker_threads_);
    return 0;
}

void Gateway::Stop() {
    if (server_) {
        server_->stop();
        server_.reset();
        NOVA_NLOG_INFO(kLogTag, "stopped");
    }
}

void Gateway::OnConnection(const hv::SocketChannelPtr& channel) {
    std::string peer = channel->peeraddr();

    if (channel->isConnected()) {
        NOVA_NLOG_INFO(kLogTag, "new connection from {}", peer);
        // 为每个连接创建 TcpConnection，附加到 channel context
        auto conn = std::make_shared<TcpConnection>(channel);
        channel->setContextPtr(conn);
        // 心跳超时：超过 heartbeat_ms_ 没有数据就关闭
        channel->setReadTimeout(heartbeat_ms_);
    } else {
        NOVA_NLOG_INFO(kLogTag, "connection closed {}", peer);
        // 清理 ConnManager
        auto conn = channel->getContextPtr<TcpConnection>();
        if (conn && conn->is_authenticated()) {
            ConnManager::Instance().Remove(conn->user_id(), conn.get());
        }
        channel->deleteContextPtr();
    }
}

void Gateway::OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
    Packet pkt;
    if (!Packet::Decode(static_cast<const char*>(buf->data()), buf->size(), pkt)) {
        NOVA_NLOG_WARN(kLogTag, "invalid packet from {}, closing", channel->peeraddr());
        channel->close();
        return;
    }

    auto conn = channel->getContextPtr<TcpConnection>();
    if (!conn) {
        channel->close();
        return;
    }

    if (handler_) {
        handler_(conn, pkt);
    }
}

} // namespace nova
