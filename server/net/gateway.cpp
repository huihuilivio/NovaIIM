#include "gateway.h"
#include "../core/server_context.h"
#include "../core/logger.h"

namespace nova {

static constexpr const char* kLogTag = "Gateway";

// body_len 字段在帧头中的偏移 = cmd(2) + seq(4) + uid(8)
static constexpr int kLengthFieldOffset = 2 + 4 + 8;

Gateway::Gateway(ServerContext& ctx) : ctx_(ctx) {}

Gateway::~Gateway() {
    Stop();
}

int Gateway::Start(int port) {
    if (server_) {
        NOVA_NLOG_WARN(kLogTag, "already started, call Stop() first");
        return -1;
    }

    server_ = std::make_unique<hv::TcpServer>();

    // 配置 UNPACK_BY_LENGTH_FIELD —— 自动拆包
    // 帧: cmd(2) + seq(4) + uid(8) + body_len(4) + body(N)
    // body_len 位于偏移 14，占 4 字节，小端序，存储 body 长度
    unpack_setting_t unpack_setting{};
    unpack_setting.mode                = UNPACK_BY_LENGTH_FIELD;
    unpack_setting.package_max_length  = kHeaderSize + kMaxBodySize;
    unpack_setting.body_offset         = static_cast<unsigned short>(kHeaderSize);
    unpack_setting.length_field_offset = kLengthFieldOffset;
    unpack_setting.length_field_bytes  = 4;
    unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;
    server_->setUnpack(&unpack_setting);

    server_->setThreadNum(worker_threads_);

    int listenfd = server_->createsocket(port);
    if (listenfd < 0) {
        NOVA_NLOG_ERROR(kLogTag, "createsocket on port {} failed (fd={})", port, listenfd);
        server_.reset();
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
        ctx_.add_connection();
        NOVA_NLOG_DEBUG(kLogTag, "new connection from {}", peer);
        // 为每个连接创建 TcpConnection，附加到 channel context
        auto conn = std::make_shared<TcpConnection>(channel);
        channel->setContextPtr(conn);
        // 心跳超时：超过 heartbeat_ms_ 没有数据就关闭
        channel->setReadTimeout(heartbeat_ms_);
    } else {
        ctx_.remove_connection();
        // 清理 ConnManager（ConnManager 自动维护在线计数）
        auto conn = channel->getContextPtr<TcpConnection>();
        if (conn && conn->is_authenticated()) {
            ctx_.conn_manager().Remove(conn->user_id(), conn.get());
        }
        channel->deleteContextPtr();
        NOVA_NLOG_DEBUG(kLogTag, "connection closed {}", peer);
    }
}

void Gateway::OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
    Packet pkt;
    if (!Packet::Decode(static_cast<const char*>(buf->data()), buf->size(), pkt)) {
        NOVA_NLOG_WARN(kLogTag, "invalid packet from {}, closing", channel->peeraddr());
        ctx_.incr_bad_packets();
        channel->close();
        return;
    }

    auto conn = channel->getContextPtr<TcpConnection>();
    if (!conn) {
        channel->close();
        return;
    }

    if (handler_) {
        ctx_.incr_messages_in();
        handler_(conn, pkt);
    }
}

}  // namespace nova
