#include "net/gateway.h"
#include "service/router.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/sync_service.h"
#include "core/thread_pool.h"
#include "core/logger.h"
#include "core/app_config.h"
#include "core/server_context.h"
#include "core/formatters.h"
#include "model/packet.h"
#include "admin/admin_server.h"
#include "dao/dao_factory.h"

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
    AppConfig cfg;
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
    NOVA_LOG_INFO("AppConfig loaded from: {}", config_path);
    NOVA_LOG_DEBUG("AppConfig:\n{}", cfg);

    // 初始化服务上下文（线程安全的运行时指标中心）
    ServerContext ctx(cfg);

    // 初始化数据库（根据配置创建对应后端的 DaoFactory）
    try {
        ctx.set_dao(CreateDaoFactory(cfg.db));
    } catch (const std::exception& e) {
        NOVA_LOG_ERROR("Failed to initialize database: {}", e.what());
        return 1;
    }
    NOVA_LOG_INFO("Database initialized ({}): {}", cfg.db.type, cfg.db.path);

    // 校验 JWT 密钥
    if (cfg.admin.enabled && !cfg.admin.jwt_secret.empty()) {
        if (cfg.admin.jwt_secret == "change-me-in-production") {
            NOVA_LOG_WARN("!!! JWT secret is still the default value. Change it in production !!!");
        }
        if (cfg.admin.jwt_secret.size() < 16) {
            NOVA_LOG_WARN("JWT secret is shorter than 16 chars, consider using a stronger secret");
        }
    }

    // 初始化服务
    UserService user_svc(ctx);
    MsgService  msg_svc(ctx);
    SyncService sync_svc(ctx);

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

    // 注册信号处理（在 Gateway 启动前，避免信号丢失）
    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // 启动 Worker 线程池
    ThreadPool worker_pool(cfg.server.worker_threads, cfg.server.queue_capacity);

    // 启动 Gateway
    Gateway gateway(ctx);
    gateway.SetWorkerThreads(cfg.server.worker_threads);
    gateway.SetPacketHandler([&](ConnectionPtr conn, Packet& pkt) {
        worker_pool.Submit([&router, conn = std::move(conn), pkt]() mutable {
            router.Dispatch(std::move(conn), pkt);
        });
    });

    int port = cfg.server.port;
    if (gateway.Start(port) != 0) {
        NOVA_LOG_ERROR("Failed to start server on port {}", port);
        return 1;
    }

    NOVA_LOG_INFO("NovaIIM Server listening on port {}", port);

    // 启动 Admin HTTP 管理面板
    std::unique_ptr<AdminServer> admin;
    if (cfg.admin.enabled) {
        admin = std::make_unique<AdminServer>(ctx);
        AdminServer::Options admin_opts;
        admin_opts.port        = cfg.admin.port;
        admin_opts.jwt_secret  = cfg.admin.jwt_secret;
        admin_opts.jwt_expires = cfg.admin.jwt_expires;
        if (admin->Start(admin_opts) != 0) {
            NOVA_LOG_WARN("Admin server failed to start, continuing without it");
            admin.reset();
        }
    }

    // 阻塞等待 SIGINT / SIGTERM
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    NOVA_LOG_INFO("Received signal {}, shutting down...", g_signal.load());
    if (admin) admin->Stop();
    gateway.Stop();
    worker_pool.Stop();
    NOVA_LOG_INFO("NovaIIM Server stopped");
    return 0;
}
