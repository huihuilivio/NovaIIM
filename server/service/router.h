#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>
#include "../net/connection.h"
#include "../model/packet.h"

namespace nova {

// 路由分发（对应架构文档 4.3 Router）
class Router {
public:
    using Handler = std::function<void(ConnectionPtr, Packet&)>;

    // 注册命令处理器（必须在 Freeze 之前调用）
    void Register(Cmd cmd, Handler handler) {
        assert(!frozen_ && "cannot register handlers after Freeze()");
        handlers_[cmd] = std::move(handler);
    }

    // 冻结路由表（启动后禁止注册，避免多线程写入）
    void Freeze() { frozen_ = true; }

    // 分发数据包
    void Dispatch(ConnectionPtr conn, Packet& pkt);

private:
    std::unordered_map<Cmd, Handler> handlers_;
    bool frozen_ = false;
};

} // namespace nova
