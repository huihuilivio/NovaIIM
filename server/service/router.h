#pragma once

#include <functional>
#include <unordered_map>
#include "../net/connection.h"
#include "../model/packet.h"

namespace nova {

// 路由分发（对应架构文档 4.3 Router）
class Router {
public:
    using Handler = std::function<void(ConnectionPtr, Packet&)>;

    // 注册命令处理器
    void Register(Cmd cmd, Handler handler) {
        handlers_[cmd] = std::move(handler);
    }

    // 分发数据包
    void Dispatch(ConnectionPtr conn, Packet& pkt);

private:
    std::unordered_map<Cmd, Handler> handlers_;
};

} // namespace nova
