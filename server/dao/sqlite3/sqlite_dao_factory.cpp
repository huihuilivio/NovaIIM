#include "sqlite_dao_factory.h"
#include "sqlite_db_manager.h"
#include "../impl/user_dao_impl.h"
#include "../impl/message_dao_impl.h"
#include "../impl/conversation_dao_impl.h"
#include "../impl/audit_log_dao_impl.h"
#include "../impl/admin_session_dao_impl.h"
#include "../impl/admin_account_dao_impl.h"
#include "../impl/rbac_dao_impl.h"
#include "../impl/file_dao_impl.h"
#include "../seed.h"

#include "../../core/logger.h"
#include <stdexcept>

namespace nova {

struct SqliteDaoFactory::Impl {
    SqliteDbManager db;
    UserDaoImplT<SqliteDbManager> user;
    MessageDaoImplT<SqliteDbManager> message;
    ConversationDaoImplT<SqliteDbManager> conversation;
    AuditLogDaoImplT<SqliteDbManager> audit_log;
    AdminSessionDaoImplT<SqliteDbManager> admin_session;
    AdminAccountDaoImplT<SqliteDbManager> admin_account;
    RbacDaoImplT<SqliteDbManager> rbac;
    FileDaoImplT<SqliteDbManager> file;

    explicit Impl(const std::string& path)
        : user(db), message(db), conversation(db), audit_log(db), admin_session(db), admin_account(db), rbac(db), file(db) {
        if (!db.Open(path)) {
            throw std::runtime_error("failed to open sqlite database: " + path);
        }
        if (!db.InitSchema()) {
            throw std::runtime_error("failed to initialize database schema");
        }
        SeedSuperAdmin(db);
        NOVA_LOG_INFO("SQLite DaoFactory initialized: {}", path);
    }

    ~Impl() { db.Close(); }
};

SqliteDaoFactory::SqliteDaoFactory(const std::string& db_path) : impl_(std::make_unique<Impl>(db_path)) {}

SqliteDaoFactory::~SqliteDaoFactory() = default;

UserDao& SqliteDaoFactory::User() {
    return impl_->user;
}
MessageDao& SqliteDaoFactory::Message() {
    return impl_->message;
}
ConversationDao& SqliteDaoFactory::Conversation() {
    return impl_->conversation;
}
AuditLogDao& SqliteDaoFactory::AuditLog() {
    return impl_->audit_log;
}
AdminSessionDao& SqliteDaoFactory::AdminSession() {
    return impl_->admin_session;
}
AdminAccountDao& SqliteDaoFactory::AdminAccount() {
    return impl_->admin_account;
}
RbacDao& SqliteDaoFactory::Rbac() {
    return impl_->rbac;
}
FileDao& SqliteDaoFactory::File() {
    return impl_->file;
}

}  // namespace nova
