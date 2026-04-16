#ifdef ORMPP_ENABLE_MYSQL

#include "mysql_db_manager.h"
#include "../../core/app_config.h"

#include "../../core/logger.h"

#include <chrono>
#include <stdexcept>

namespace nova {

static constexpr const char* kLogTag = "MysqlDB";

// ---- 线程级连接固定 ----
static thread_local ormpp::dbng<ormpp::mysql>* g_pinned = nullptr;

/// RAII 会话实现：固定一条连接到当前线程
class MysqlScopedConn final : public DaoScopedConn {
public:
    explicit MysqlScopedConn(MysqlDbManager& mgr) : mgr_(mgr) {
        if (!g_pinned) {
            owned_   = mgr_.AcquireFromPool();
            g_pinned = owned_.get();
        }
    }

    ~MysqlScopedConn() override {
        if (owned_) {
            g_pinned = nullptr;
            mgr_.ReturnConn(std::move(owned_));
        }
    }

private:
    MysqlDbManager& mgr_;
    std::unique_ptr<ormpp::dbng<ormpp::mysql>> owned_;
};

// ---- ConnGuard ----

MysqlDbManager::ConnGuard::ConnGuard(MysqlDbManager* mgr, std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn)
    : mgr_(mgr), owned_(std::move(conn)), conn_(owned_.get()) {}

MysqlDbManager::ConnGuard::ConnGuard(ormpp::dbng<ormpp::mysql>* borrowed) : conn_(borrowed) {}

MysqlDbManager::ConnGuard::~ConnGuard() {
    if (owned_ && mgr_) {
        mgr_->ReturnConn(std::move(owned_));
    }
}

MysqlDbManager::ConnGuard::ConnGuard(ConnGuard&& o) noexcept
    : mgr_(o.mgr_), owned_(std::move(o.owned_)), conn_(o.conn_) {
    o.mgr_  = nullptr;
    o.conn_ = nullptr;
}

// ---- MysqlDbManager ----

MysqlDbManager::~MysqlDbManager() {
    Close();
}

std::unique_ptr<ormpp::dbng<ormpp::mysql>> MysqlDbManager::NewConn() {
    auto conn = std::make_unique<ormpp::dbng<ormpp::mysql>>();
    if (!conn->connect(host_, user_, passwd_, dbname_, std::optional<int>(10), std::optional<int>(port_))) {
        NOVA_NLOG_ERROR(kLogTag, "MySQL connect failed: {}:{}@{}:{}/{}", user_, "***", host_, port_, dbname_);
        return nullptr;
    }
    return conn;
}

bool MysqlDbManager::Open(const DatabaseConfig& config) {
    host_      = config.host;
    port_      = config.port;
    user_      = config.user;
    passwd_    = config.password;
    dbname_    = config.database;
    pool_size_ = config.pool_size;
    closed_    = false;

    std::scoped_lock lock(mutex_);
    for (int i = 0; i < pool_size_; ++i) {
        auto conn = NewConn();
        if (!conn) {
            NOVA_NLOG_ERROR(kLogTag, "Failed to create MySQL connection {}/{}", i + 1, pool_size_);
            return false;
        }
        pool_.push_back(std::move(conn));
    }
    NOVA_NLOG_INFO(kLogTag, "MySQL connection pool initialized: {}:{}/{} (pool_size={})", host_, port_, dbname_, pool_size_);
    return true;
}

void MysqlDbManager::Close() {
    std::scoped_lock lock(mutex_);
    closed_ = true;
    pool_.clear();
    cv_.notify_all();
    NOVA_NLOG_INFO(kLogTag, "MySQL connection pool closed");
}

void MysqlDbManager::ReturnConn(std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn) {
    if (!conn)
        return;
    {
        std::scoped_lock lock(mutex_);
        if (closed_)
            return;
        if (!conn->ping()) {
            NOVA_NLOG_WARN(kLogTag, "MySQL connection lost, discarding");
            return;
        }
        pool_.push_back(std::move(conn));
    }
    cv_.notify_one();
}

MysqlDbManager::ConnGuard MysqlDbManager::DB() {
    // 若当前线程已固定连接（Session 作用域内），返回 borrowed 守卫
    if (g_pinned) {
        return ConnGuard(g_pinned);
    }
    // 否则从池中获取 owning 守卫
    return ConnGuard(this, AcquireFromPool());
}

std::unique_ptr<ormpp::dbng<ormpp::mysql>> MysqlDbManager::AcquireFromPool() {
    std::unique_lock lock(mutex_);

    if (pool_.empty()) {
        if (!cv_.wait_for(lock, std::chrono::seconds(3), [this] { return !pool_.empty() || closed_; })) {
            throw std::runtime_error("MySQL connection pool exhausted (timeout 3s)");
        }
    }
    if (closed_ || pool_.empty()) {
        throw std::runtime_error("MySQL connection pool is closed");
    }

    auto conn = std::move(pool_.front());
    pool_.pop_front();
    lock.unlock();

    if (!conn->ping()) {
        NOVA_NLOG_WARN(kLogTag, "MySQL connection stale, reconnecting...");
        conn = NewConn();
        if (!conn) {
            throw std::runtime_error("MySQL reconnect failed");
        }
    }

    return conn;
}

std::unique_ptr<DaoScopedConn> MysqlDbManager::CreateSession() {
    return std::make_unique<MysqlScopedConn>(*this);
}

bool MysqlDbManager::InitSchema() {
    auto db = DB();
    bool ok = true;

    ok = ok && db.create_datatable<User>(ormpp_auto_key{"id"}, ormpp_unique{{"uid"}}, ormpp_unique{{"email"}});
    ok = ok && db.create_datatable<Admin>(ormpp_auto_key{"id"}, ormpp_unique{{"uid"}});
    ok = ok && db.create_datatable<UserDevice>(ormpp_auto_key{"id"}, ormpp_unique{{"user_id", "device_id"}});
    ok = ok && db.create_datatable<Message>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<Conversation>(ormpp_auto_key{"id"});
    ok = ok &&
         db.create_datatable<ConversationMember>(ormpp_auto_key{"id"}, ormpp_unique{{"conversation_id", "user_id"}});
    ok = ok && db.create_datatable<AuditLog>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<AdminSession>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<Role>(ormpp_auto_key{"id"}, ormpp_unique{{"code"}});
    ok = ok && db.create_datatable<Permission>(ormpp_auto_key{"id"}, ormpp_unique{{"code"}});
    ok = ok && db.create_datatable<RolePermission>(ormpp_auto_key{"id"}, ormpp_unique{{"role_id", "permission_id"}});
    ok = ok && db.create_datatable<AdminRole>(ormpp_auto_key{"id"}, ormpp_unique{{"admin_id", "role_id"}});
    ok = ok && db.create_datatable<UserFile>(ormpp_auto_key{"id"});

    db.execute("CREATE INDEX IF NOT EXISTS idx_msg_conv_time ON messages(conversation_id, created_at)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_msg_conv_seq ON messages(conversation_id, seq)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_msg_sender ON messages(sender_id)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_convmember_user ON conversation_members(user_id)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_audit_admin_action ON audit_logs(admin_id, action)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_audit_created ON audit_logs(created_at)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_session_token ON admin_sessions(token_hash)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_session_admin ON admin_sessions(admin_id)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_userfile_user_type ON user_files(user_id, file_type)");

    if (!ok) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to initialize MySQL schema");
    } else {
        NOVA_NLOG_INFO(kLogTag, "MySQL schema initialized");
    }
    return ok;
}

}  // namespace nova

#endif  // ORMPP_ENABLE_MYSQL
