#include "net/gateway.h"
#include "service/router.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/sync_service.h"
#include "core/thread_pool.h"
#include "core/logger.h"
#include "core/config.h"
#include "model/packet.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace nova;

static void PrintUsage(const char* prog) {
    std::fprintf(stderr, "Usage: %s -c <config.yaml>\n", prog);
}

int main(int argc, char* argv[]) {
    // ---- 解析命令行参数 ----
    std::string config_path;
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--config") == 0)
            && i + 1 < argc) {
            config_path = argv[++i];
        } else {
            PrintUsage(argv[0]);
            return 1;
        }
    }
    if (config_path.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }

    // ---- 加载配置 ----
    Config cfg;
    if (!LoadConfig(cfg, config_path)) {
        return 1;
    }

    // ---- 初始化日志（在配置加载之后）----
    nova::log::LogOptions log_opts;
    log_opts.level          = spdlog::level::from_str(cfg.log.level);
    log_opts.flush_level    = spdlog::level::from_str(cfg.log.flush_level);
    log_opts.pattern        = cfg.log.pattern;
    log_opts.file           = cfg.log.file;
    log_opts.max_size       = static_cast<std::size_t>(cfg.log.max_size) * 1024 * 1024;
    log_opts.max_files      = static_cast<std::size_t>(cfg.log.max_files);
    log_opts.rotate_on_open = cfg.log.rotate_on_open;
    nova::log::Init(log_opts);
    NOVA_LOG_INFO("NovaIIM Server starting...");
    NOVA_LOG_INFO("Config loaded from: {}", config_path);

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

    int port = cfg.server.port;
    if (gateway.Start(port) != 0) {
        NOVA_LOG_ERROR("Failed to start server on port {}", port);
        return 1;
    }

    NOVA_LOG_INFO("NovaIIM Server listening on port {}", port);

    // TODO: 阻塞等待信号 (SIGINT/SIGTERM)
    // gateway.Stop();

    return 0;
}
