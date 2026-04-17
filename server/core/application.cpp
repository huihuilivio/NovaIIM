#include "application.h"
#include "logger.h"
#include "server_context.h"
#include "thread_pool.h"
#include "formatters.h"
#include "../net/gateway.h"
#include "../service/router.h"
#include "../service/user_service.h"
#include "../service/msg_service.h"
#include "../service/sync_service.h"
#include "../service/errors/common.h"
#include "../admin/admin_server.h"
#include "../dao/dao_factory.h"
#include <nova/packet.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace nova {

static constexpr char kLogTag[] = "App";

// ---- 信号处理 ----
static std::atomic<bool> g_running{true};
static std::atomic<int> g_signal{0};

static void SignalHandler(int sig) {
    g_signal.store(sig, std::memory_order_relaxed);
    g_running.store(false, std::memory_order_relaxed);
}

// ---- 内部辅助 ----

static size_t RoundUpPow2(size_t n) {
    if (n < 2) return 2;
    if ((n & (n - 1)) == 0) return n;
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

static void InitLog(const AppConfig& cfg) {
    log::Init({
        .level          = spdlog::level::from_str(cfg.log.level),
        .flush_level    = spdlog::level::from_str(cfg.log.flush_level),
        .pattern        = cfg.log.pattern,
        .file           = cfg.log.file,
        .max_size       = static_cast<std::size_t>(cfg.log.max_size) * 1024 * 1024,
        .max_files      = static_cast<std::size_t>(cfg.log.max_files),
        .rotate_on_open = cfg.log.rotate_on_open,
    });
}

static bool InitDatabase(ServerContext& ctx, const DatabaseConfig& db_cfg) {
    try {
        ctx.set_dao(CreateDaoFactory(db_cfg));
    } catch (const std::exception& e) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to initialize database: {}", e.what());
        return false;
    }
    NOVA_NLOG_INFO(kLogTag, "Database initialized ({}): {}", db_cfg.type, db_cfg.path);
    return true;
}

static void WarnJwtSecret(const AdminConfig& admin_cfg) {
    if (!admin_cfg.enabled || admin_cfg.jwt_secret.empty()) return;
    if (admin_cfg.jwt_secret == "change-me-in-production") {
        NOVA_NLOG_WARN(kLogTag, "!!! JWT secret is still the default value. Change it in production !!!");
    }
    if (admin_cfg.jwt_secret.size() < 16) {
        NOVA_NLOG_WARN(kLogTag, "JWT secret is shorter than 16 chars, consider using a stronger secret");
    }
}

static void RegisterRoutes(Router& router, UserService& user_svc, MsgService& msg_svc, SyncService& sync_svc) {
    router.Register(Cmd::kLogin,    [&](ConnectionPtr c, Packet& p) { user_svc.HandleLogin(c, p); });
    router.Register(Cmd::kRegister, [&](ConnectionPtr c, Packet& p) { user_svc.HandleRegister(c, p); });
    router.Register(Cmd::kLogout,   [&](ConnectionPtr c, Packet& p) { user_svc.HandleLogout(c, p); });
    router.Register(Cmd::kHeartbeat,[&](ConnectionPtr c, Packet& p) { user_svc.HandleHeartbeat(c, p); });
    router.Register(Cmd::kSearchUser,    [&](ConnectionPtr c, Packet& p) { user_svc.HandleSearchUser(c, p); });
    router.Register(Cmd::kGetUserProfile,[&](ConnectionPtr c, Packet& p) { user_svc.HandleGetProfile(c, p); });
    router.Register(Cmd::kUpdateProfile, [&](ConnectionPtr c, Packet& p) { user_svc.HandleUpdateProfile(c, p); });

    router.Register(Cmd::kSendMsg,    [&](ConnectionPtr c, Packet& p) { msg_svc.HandleSendMsg(c, p); });
    router.Register(Cmd::kDeliverAck, [&](ConnectionPtr c, Packet& p) { msg_svc.HandleDeliverAck(c, p); });
    router.Register(Cmd::kReadAck,    [&](ConnectionPtr c, Packet& p) { msg_svc.HandleReadAck(c, p); });

    router.Register(Cmd::kSyncMsg,    [&](ConnectionPtr c, Packet& p) { sync_svc.HandleSyncMsg(c, p); });
    router.Register(Cmd::kSyncUnread, [&](ConnectionPtr c, Packet& p) { sync_svc.HandleSyncUnread(c, p); });

    router.Freeze();
}

static Gateway::PacketHandler MakePacketHandler(Router& router, ThreadPool& pool, ServerContext& ctx) {
    return [&](ConnectionPtr conn, Packet& pkt) {
        if (!pool.Submit([&router, conn, pkt]() mutable { router.Dispatch(std::move(conn), pkt); })) {
            NOVA_NLOG_WARN(kLogTag, "worker queue full, rejecting cmd=0x{:04X}", pkt.cmd);
            Packet err;
            err.cmd  = pkt.cmd;
            err.seq  = pkt.seq;
            err.uid  = pkt.uid;
            err.body = proto::Serialize(proto::RspBase{errc::kServerBusy.code, errc::kServerBusy.msg});
            conn->Send(err);
            ctx.incr_bad_packets();
        }
    };
}

// ---- Application::Run ----

int Application::Run(const AppConfig& cfg) {
    // 1. 日志
    InitLog(cfg);
    NOVA_NLOG_INFO(kLogTag, "NovaIIM Server starting...");
    NOVA_NLOG_DEBUG(kLogTag, "AppConfig:\n{}", cfg);

    // 2. 服务上下文 + 数据库
    ServerContext ctx(cfg);
    if (!InitDatabase(ctx, cfg.db)) return 1;

    // 3. JWT 安全检查
    WarnJwtSecret(cfg.admin);

    // 4. 服务 + 路由
    UserService user_svc(ctx);
    MsgService msg_svc(ctx);
    SyncService sync_svc(ctx);

    Router router;
    RegisterRoutes(router, user_svc, msg_svc, sync_svc);

    // 5. 信号处理
    g_running.store(true, std::memory_order_relaxed);
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // 6. Worker 线程池
    auto qcap = RoundUpPow2(static_cast<size_t>(cfg.server.queue_capacity));
    if (qcap != static_cast<size_t>(cfg.server.queue_capacity)) {
        NOVA_NLOG_WARN(kLogTag, "queue_capacity {} is not a power of 2, rounded up to {}", cfg.server.queue_capacity, qcap);
    }
    ThreadPool worker_pool(cfg.server.worker_threads, qcap);

    // 7. Gateway
    Gateway gateway(ctx);
    gateway.SetWorkerThreads(cfg.server.worker_threads);
    gateway.SetHeartbeatInterval(cfg.server.heartbeat_ms);
    gateway.SetPacketHandler(MakePacketHandler(router, worker_pool, ctx));

    if (gateway.Start(cfg.server.port) != 0) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to start server on port {}", cfg.server.port);
        return 1;
    }
    NOVA_NLOG_INFO(kLogTag, "NovaIIM Server listening on port {}", cfg.server.port);

    // 8. Admin HTTP 面板
    std::unique_ptr<AdminServer> admin;
    if (cfg.admin.enabled) {
        admin = std::make_unique<AdminServer>(ctx);
        AdminServer::Options opts;
        opts.port        = cfg.admin.port;
        opts.jwt_secret  = cfg.admin.jwt_secret;
        opts.jwt_expires = cfg.admin.jwt_expires;
        if (admin->Start(opts) != 0) {
            NOVA_NLOG_WARN(kLogTag, "Admin server failed to start, continuing without it");
            admin.reset();
        }
    }

    // 9. 等待退出信号
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 10. 有序关闭：Admin → Gateway → ThreadPool
    NOVA_NLOG_INFO(kLogTag, "Received signal {}, shutting down...", g_signal.load());
    if (admin) admin->Stop();
    gateway.Stop();
    worker_pool.Stop();
    NOVA_NLOG_INFO(kLogTag, "NovaIIM Server stopped");
    return 0;
}

}  // namespace nova
