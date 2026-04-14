#include "net/gateway.h"
#include "service/router.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/sync_service.h"
#include "core/thread_pool.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/formatters.h"
#include "model/packet.h"

#include <CLI/CLI.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>

using namespace nova;

// ---- 信号处理 ----
static std::atomic<bool> g_running{true};
static std::atomic<int>  g_signal{0};

static void SignalHandler(int sig) {
    // 仅设置原子标志，不调用任何非 async-signal-safe 函数
    g_signal.store(sig, std::memory_order_relaxed);
    g_running.store(false, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    // ---- 命令行参数 ----
    CLI::App app{"NovaIIM Server - High-performance IM backend"};
    app.set_version_flag("-v,--version", "0.1.0");

    std::string config_path;
    app.add_option("-c,--config", config_path, "Path to config YAML file")
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    // ---- 加载配置 ----
    Config cfg;
    if (!LoadConfig(cfg, config_path)) {
        return 1;
    }

    // ---- 初始化日志（在配置加载之后）----
    nova::log::Init({
        .level          = spdlog::level::from_str(cfg.log.level),
        .flush_level    = spdlog::level::from_str(cfg.log.flush_level),
        .pattern        = cfg.log.pattern,
        .file           = cfg.log.file,
        .max_size       = static_cast<std::size_t>(cfg.log.max_size) * 1024 * 1024,
        .max_files      = static_cast<std::size_t>(cfg.log.max_files),
        .rotate_on_open = cfg.log.rotate_on_open,
    });
    NOVA_LOG_INFO("NovaIIM Server starting...");
    NOVA_LOG_INFO("Config loaded from: {}", config_path);
    NOVA_LOG_DEBUG("Config:\n{}", cfg);

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
    gateway.SetWorkerThreads(cfg.server.worker_threads);
    gateway.SetPacketHandler([&](ConnectionPtr conn, Packet& pkt) {
        router.Dispatch(std::move(conn), pkt);
    });

    int port = cfg.server.port;
    if (gateway.Start(port) != 0) {
        NOVA_LOG_ERROR("Failed to start server on port {}", port);
        return 1;
    }

    NOVA_LOG_INFO("NovaIIM Server listening on port {}", port);

    // 阻塞等待 SIGINT / SIGTERM
    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    NOVA_LOG_INFO("Received signal {}, shutting down...", g_signal.load());
    gateway.Stop();
    NOVA_LOG_INFO("NovaIIM Server stopped");
    return 0;
}
