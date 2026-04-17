#include "sqlite_db_manager.h"

#include "../../core/logger.h"

namespace nova {

static constexpr const char* kLogTag = "SqliteDB";

bool SqliteDbManager::Open(const std::string& path) {
    if (!db_.connect(path)) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to open database: {}", path);
        return false;
    }

    // 启用 WAL 模式 + 外键约束
    if (!db_.execute("PRAGMA journal_mode=WAL")) {
        NOVA_NLOG_WARN(kLogTag, "Failed to enable WAL mode");
    }
    if (!db_.execute("PRAGMA foreign_keys=ON")) {
        NOVA_NLOG_WARN(kLogTag, "Failed to enable foreign key constraints");
    }
    if (!db_.execute("PRAGMA busy_timeout=5000")) {
        NOVA_NLOG_WARN(kLogTag, "Failed to set busy_timeout");
    }

    NOVA_NLOG_INFO(kLogTag, "Database opened: {}", path);
    return true;
}

void SqliteDbManager::Close() {
    db_.disconnect();
    NOVA_NLOG_INFO(kLogTag, "Database closed");
}

bool SqliteDbManager::InitSchema() {
    bool ok = true;

    ok = ok && db_.create_datatable<User>(ormpp_auto_key{"id"}, ormpp_unique{{"uid"}}, ormpp_unique{{"email"}});
    ok = ok && db_.create_datatable<Admin>(ormpp_auto_key{"id"}, ormpp_unique{{"uid"}});
    ok = ok && db_.create_datatable<UserDevice>(ormpp_auto_key{"id"}, ormpp_unique{{"uid", "device_id"}});
    ok = ok && db_.create_datatable<Message>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<Conversation>(ormpp_auto_key{"id"});
    ok = ok &&
         db_.create_datatable<ConversationMember>(ormpp_auto_key{"id"}, ormpp_unique{{"conversation_id", "user_id"}});
    ok = ok && db_.create_datatable<AuditLog>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<AdminSession>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<Role>(ormpp_auto_key{"id"}, ormpp_unique{{"code"}});
    ok = ok && db_.create_datatable<Permission>(ormpp_auto_key{"id"}, ormpp_unique{{"code"}});
    ok = ok && db_.create_datatable<RolePermission>(ormpp_auto_key{"id"}, ormpp_unique{{"role_id", "permission_id"}});
    ok = ok && db_.create_datatable<AdminRole>(ormpp_auto_key{"id"}, ormpp_unique{{"admin_id", "role_id"}});
    ok = ok && db_.create_datatable<UserFile>(ormpp_auto_key{"id"});

    db_.execute("CREATE INDEX IF NOT EXISTS idx_msg_conv_time ON messages(conversation_id, created_at)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_msg_conv_seq ON messages(conversation_id, seq)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_msg_sender ON messages(sender_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_convmember_user ON conversation_members(user_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_audit_admin_action ON audit_logs(admin_id, action)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_audit_created ON audit_logs(created_at)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_session_token ON admin_sessions(token_hash)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_session_admin ON admin_sessions(admin_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_userfile_user_type ON user_files(user_id, file_type)");

    if (!ok) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to initialize database schema");
    } else {
        NOVA_NLOG_INFO(kLogTag, "Database schema initialized");
    }
    return ok;
}

}  // namespace nova
