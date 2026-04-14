#pragma once

#include <functional>
#include "connection.h"
#include "../model/packet.h"

namespace nova {

// Gateway 网关（对应架构文档 4.1）
// 职责：管理连接、WebSocket/TCP 收发、心跳检测
class Gateway {
public:
    using PacketHandler = std::function<void(ConnectionPtr, Packet&)>;

    Gateway() = default;
    ~Gateway() = default;

    // 设置数据包处理回调（由 Router 注册）
    void SetPacketHandler(PacketHandler handler) { handler_ = std::move(handler); }

    // 启动监听
    // port: 监听端口
    // 返回 0 成功，-1 失败
    int Start(int port);

    // 停止服务
    void Stop();

private:
    // 心跳超时检测
    void CheckHeartbeat();

    PacketHandler handler_;
    int           port_ = 0;
    bool          running_ = false;
};

} // namespace nova
