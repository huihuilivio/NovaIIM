#include "gateway.h"

namespace nova {

int Gateway::Start(int port) {
    port_ = port;
    running_ = true;
    // TODO: 初始化 libhv WebSocket server
    // TODO: 注册 onConnection / onMessage / onClose 回调
    // TODO: 启动心跳检测定时器
    return 0;
}

void Gateway::Stop() {
    running_ = false;
    // TODO: 关闭 libhv server
}

void Gateway::CheckHeartbeat() {
    // TODO: 遍历连接，关闭超时连接
}

} // namespace nova
