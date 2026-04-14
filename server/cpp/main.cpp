#include "net/gateway.h"
#include "service/router.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/sync_service.h"
#include "core/thread_pool.h"
#include "model/packet.h"
// #include <spdlog/spdlog.h>

#include <cstdio>

using namespace nova;

int main() {
    // spdlog::set_level(spdlog::level::info);
    // spdlog::info("NovaIIM Server starting...");
    std::printf("NovaIIM Server starting...\n");

    // 初始化服务
    UserService user_svc;
    MsgService  msg_svc;
    SyncService sync_svc;

    // 注册路由
    Router router;
    router.Register(Cmd::kLogin,      [&](ConnectionPtr c, Packet& p) { user_svc.HandleLogin(c, p); });
    router.Register(Cmd::kLogout,     [&](ConnectionPtr c, Packet& p) { user_svc.HandleLogout(c, p); });
    router.Register(Cmd::kHeartbeat,  [&](ConnectionPtr c, Packet& p) { user_svc.HandleHeartbeat(c, p); });

    router.Register(Cmd::kSendMsg,    [&](ConnectionPtr c, Packet& p) { msg_svc.HandleSendMsg(c, p); });
    router.Register(Cmd::kDeliverAck, [&](ConnectionPtr c, Packet& p) { msg_svc.HandleDeliverAck(c, p); });
    router.Register(Cmd::kReadAck,    [&](ConnectionPtr c, Packet& p) { msg_svc.HandleReadAck(c, p); });

    router.Register(Cmd::kSyncMsg,    [&](ConnectionPtr c, Packet& p) { sync_svc.HandleSyncMsg(c, p); });
    router.Register(Cmd::kSyncUnread, [&](ConnectionPtr c, Packet& p) { sync_svc.HandleSyncUnread(c, p); });

    // 启动 Gateway
    Gateway gateway;
    gateway.SetPacketHandler([&](ConnectionPtr conn, Packet& pkt) {
        router.Dispatch(std::move(conn), pkt);
    });

    int port = 9090;
    if (gateway.Start(port) != 0) {
        std::printf("Failed to start server on port %d\n", port);
        return 1;
    }

    std::printf("NovaIIM Server listening on port %d\n", port);

    // TODO: 阻塞等待信号 (SIGINT/SIGTERM)
    // gateway.Stop();

    return 0;
}
