#pragma once

#include <functional>
#include <memory>
#include "tcp_connection.h"
#include <nova/packet.h>

#include <hv/TcpServer.h>

namespace nova {

class ServerContext;

// Gateway 网关（对应架构文档 4.1）
// 职责：管理 TCP 连接、协议拆包、心跳超时
class Gateway {
public:
    using PacketHandler = std::function<void(ConnectionPtr, Packet&)>;

    explicit Gateway(ServerContext& ctx);
    ~Gateway();

    // 禁止拷贝/移动（回调中捕获了 this）
    Gateway(const Gateway&)            = delete;
    Gateway& operator=(const Gateway&) = delete;

    // 设置数据包处理回调（由 Router 注册，必须在 Start 之前调用）
    void SetPacketHandler(PacketHandler handler) { handler_ = std::move(handler); }

    // 配置参数（在 Start 之前调用）
    void SetWorkerThreads(int n) { worker_threads_ = n; }
    void SetHeartbeatInterval(int ms) { heartbeat_ms_ = ms; }

    // 启动监听，返回 0 成功，<0 失败
    int Start(int port);

    // 停止服务
    void Stop();

private:
    void OnConnection(const hv::SocketChannelPtr& channel);
    void OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* buf);

    PacketHandler handler_;
    std::unique_ptr<hv::TcpServer> server_;
    ServerContext& ctx_;
    int worker_threads_ = 4;
    int heartbeat_ms_   = 30000;  // 30s 心跳超时
};

}  // namespace nova
