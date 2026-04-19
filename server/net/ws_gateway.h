#pragma once

#include <functional>
#include <memory>
#include "ws_connection.h"
#include <nova/packet.h>

#include <hv/WebSocketServer.h>

namespace nova {

class ServerContext;

// WsGateway —— WebSocket 网关
// 与 TCP Gateway 并行运行，共享 Router / ConnManager / ThreadPool
// 职责：管理 WebSocket 连接、协议解码、心跳（libhv 内置 ping/pong）
class WsGateway {
public:
    using PacketHandler = std::function<void(ConnectionPtr, Packet&)>;

    explicit WsGateway(ServerContext& ctx);
    ~WsGateway();

    // 禁止拷贝/移动
    WsGateway(const WsGateway&)            = delete;
    WsGateway& operator=(const WsGateway&) = delete;

    void SetPacketHandler(PacketHandler handler) { handler_ = std::move(handler); }

    // 心跳 ping 间隔（毫秒），默认 25s（略小于 TCP 的 30s 超时）
    void SetPingInterval(int ms) { ping_interval_ms_ = ms; }

    // 启动监听，返回 0 成功，<0 失败
    int Start(int port);

    // 停止服务
    void Stop();

private:
    PacketHandler handler_;
    hv::WebSocketService ws_service_;
    std::unique_ptr<hv::WebSocketServer> server_;
    ServerContext& ctx_;
    int ping_interval_ms_ = 25000;
};

}  // namespace nova
