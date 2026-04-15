#ifdef ORMPP_ENABLE_MYSQL

#include "mysql_db_manager.h"
#include "../../core/app_config.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <stdexcept>

namespace nova {

// ---- ConnGuard ----

MysqlDbManager::ConnGuard::ConnGuard(MysqlDbManager* mgr,
                                      std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn)
    : mgr_(mgr), conn_(std::move(conn)) {}

MysqlDbManager::ConnGuard::~ConnGuard() {
    if (conn_ && mgr_) {
        mgr_->ReturnConn(std::move(conn_));
    }
}

MysqlDbManager::ConnGuard::ConnGuard(ConnGuard&& o) noexcept
    : mgr_(o.mgr_), conn_(std::move(o.conn_)) {
    o.mgr_ = nullptr;
}

// ---- MysqlDbManager ----

MysqlDbManager::~MysqlDbManager() {
    Close();
}

std::unique_ptr<ormpp::dbng<ormpp::mysql>> MysqlDbManager::NewConn() {
    auto conn = std::make_unique<ormpp::dbng<ormpp::mysql>>();
    if (!conn->connect(host_, user_, passwd_, dbname_,
                       std::optional<int>(10),
                       std::optional<int>(port_))) {
        SPDLOG_ERROR("MySQL connect failed: {}:{}@{}:{}/{}",
                     user_, "***", host_, port_, dbname_);
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
            SPDLOG_ERROR("Failed to create MySQL connection {}/{}", i + 1, pool_size_);
            return false;
        }
        pool_.push_back(std::move(conn));
    }
    SPDLOG_INFO("MySQL connection pool initialized: {}:{}/{} (pool_size={})",
                host_, port_, dbname_, pool_size_);
    return true;
}

void MysqlDbManager::Close() {
    std::scoped_lock lock(mutex_);
    closed_ = true;
    pool_.clear();
    cv_.notify_all();
    SPDLOG_INFO("MySQL connection pool closed");
}

void MysqlDbManager::ReturnConn(std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn) {
    if (!conn) return;
    {
        std::scoped_lock lock(mutex_);
        if (closed_) return;
        if (!conn->ping()) {
            SPDLOG_WARN("MySQL connection lost, discarding");
            return;
        }
        pool_.push_back(std::move(conn));
    }
    cv_.notify_one();
}

MysqlDbManager::ConnGuard MysqlDbManager::DB() {
    std::unique_lock lock(mutex_);

    if (pool_.empty()) {
        if (!cv_.wait_for(lock, std::chrono::seconds(3),
                          [this] { return !pool_.empty() || closed_; })) {
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
        SPDLOG_WARN("MySQL connection stale, reconnecting...");
        conn = NewConn();
        if (!conn) {
            throw std::runtime_error("MySQL reconnect failed");
        }
    }

    return ConnGuard(this, std::move(conn));
}

bool MysqlDbManager::InitSchema() {
    auto db = DB();
    bool ok = true;

    ok = ok && db.create_datatable<User>(ormpp_auto_key{"id"},
                ormpp_unique{{"uid"}});
    ok = ok && db.create_datatable<UserDevice>(ormpp_auto_key{"id"},
                ormpp_unique{{"user_id", "device_id"}});
    ok = ok && db.create_datatable<Message>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<Conversation>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<ConversationMember>(ormpp_auto_key{"id"},
                ormpp_unique{{"conversation_id", "user_id"}});
    ok = ok && db.create_datatable<AuditLog>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<AdminSession>(ormpp_auto_key{"id"});
    ok = ok && db.create_datatable<Role>(ormpp_auto_key{"id"},
                ormpp_unique{{"code"}});
    ok = ok && db.create_datatable<Permission>(ormpp_auto_key{"id"},
                ormpp_unique{{"code"}});
    ok = ok && db.create_datatable<RolePermission>(ormpp_auto_key{"id"},
                ormpp_unique{{"role_id", "permission_id"}});
    ok = ok && db.create_datatable<UserRole>(ormpp_auto_key{"id"},
                ormpp_unique{{"user_id", "role_id"}});

    db.execute("CREATE INDEX IF NOT EXISTS idx_msg_conv_time ON messages(conversation_id, created_at)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_msg_sender ON messages(sender_id)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_audit_user_action ON audit_logs(user_id, action)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_audit_created ON audit_logs(created_at)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_session_token ON admin_sessions(token_hash)");
    db.execute("CREATE INDEX IF NOT EXISTS idx_session_user ON admin_sessions(user_id)");

    if (!ok) {
        SPDLOG_ERROR("Failed to initialize MySQL schema");
    } else {
        SPDLOG_INFO("MySQL schema initialized");
    }
    return ok;
}

}  // namespace nova

#endif  // ORMPP_ENABLE_MYSQL
