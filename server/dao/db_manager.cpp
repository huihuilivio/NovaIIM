#include "db_manager.h"

#include <spdlog/spdlog.h>

namespace nova {

bool DbManager::Open(const std::string& path) {
    if (!db_.connect(path)) {
        SPDLOG_ERROR("Failed to open database: {}", path);
        return false;
    }

    // 启用 WAL 模式 + 外键约束
    if (!db_.execute("PRAGMA journal_mode=WAL")) {
        SPDLOG_WARN("Failed to enable WAL mode");
    }
    if (!db_.execute("PRAGMA foreign_keys=ON")) {
        SPDLOG_WARN("Failed to enable foreign key constraints");
    }
    if (!db_.execute("PRAGMA busy_timeout=5000")) {
        SPDLOG_WARN("Failed to set busy_timeout");
    }

    SPDLOG_INFO("Database opened: {}", path);
    return true;
}

void DbManager::Close() {
    db_.disconnect();
    SPDLOG_INFO("Database closed");
}

bool DbManager::InitSchema() {
    // ormpp create_datatable 自动生成 CREATE TABLE IF NOT EXISTS
    bool ok = true;

    ok = ok && db_.create_datatable<User>(ormpp_auto_key{"id"},
                ormpp_unique{{"uid"}});
    ok = ok && db_.create_datatable<UserDevice>(ormpp_auto_key{"id"},
                ormpp_unique{{"user_id", "device_id"}});
    ok = ok && db_.create_datatable<Message>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<Conversation>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<ConversationMember>(ormpp_auto_key{"id"},
                ormpp_unique{{"conversation_id", "user_id"}});
    ok = ok && db_.create_datatable<AuditLog>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<AdminSession>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<Role>(ormpp_auto_key{"id"},
                ormpp_unique{{"code"}});
    ok = ok && db_.create_datatable<Permission>(ormpp_auto_key{"id"},
                ormpp_unique{{"code"}});
    ok = ok && db_.create_datatable<RolePermission>(ormpp_auto_key{"id"},
                ormpp_unique{{"role_id", "permission_id"}});
    ok = ok && db_.create_datatable<UserRole>(ormpp_auto_key{"id"},
                ormpp_unique{{"user_id", "role_id"}});

    // ormpp 不直接支持 CREATE INDEX，手动补充
    db_.execute("CREATE INDEX IF NOT EXISTS idx_msg_conv_time ON messages(conversation_id, created_at)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_msg_sender ON messages(sender_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_audit_user_action ON audit_logs(user_id, action)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_audit_created ON audit_logs(created_at)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_session_token ON admin_sessions(token_hash)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_session_user ON admin_sessions(user_id)");

    if (!ok) {
        SPDLOG_ERROR("Failed to initialize database schema");
    } else {
        SPDLOG_INFO("Database schema initialized");
    }
    return ok;
}

} // namespace nova
