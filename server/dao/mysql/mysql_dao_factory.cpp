#ifdef ORMPP_ENABLE_MYSQL

#include "mysql_dao_factory.h"
#include "mysql_db_manager.h"
#include "../impl/user_dao_impl.h"
#include "../impl/message_dao_impl.h"
#include "../impl/audit_log_dao_impl.h"
#include "../impl/admin_session_dao_impl.h"
#include "../impl/rbac_dao_impl.h"
#include "../../core/app_config.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace nova {

struct MysqlDaoFactory::Impl {
    MysqlDbManager                       db;
    UserDaoImplT<MysqlDbManager>         user;
    MessageDaoImplT<MysqlDbManager>      message;
    AuditLogDaoImplT<MysqlDbManager>     audit_log;
    AdminSessionDaoImplT<MysqlDbManager> admin_session;
    RbacDaoImplT<MysqlDbManager>         rbac;

    explicit Impl(const DatabaseConfig& config)
        : user(db), message(db), audit_log(db),
          admin_session(db), rbac(db) {
        if (!db.Open(config)) {
            throw std::runtime_error("failed to open MySQL connection pool");
        }
        if (!db.InitSchema()) {
            throw std::runtime_error("failed to initialize MySQL schema");
        }
        SPDLOG_INFO("MySQL DaoFactory initialized: {}:{}/{}",
                     config.host, config.port, config.database);
    }

    ~Impl() {
        db.Close();
    }
};

MysqlDaoFactory::MysqlDaoFactory(const DatabaseConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

MysqlDaoFactory::~MysqlDaoFactory() = default;

UserDao&         MysqlDaoFactory::User()         { return impl_->user; }
MessageDao&      MysqlDaoFactory::Message()      { return impl_->message; }
AuditLogDao&     MysqlDaoFactory::AuditLog()     { return impl_->audit_log; }
AdminSessionDao& MysqlDaoFactory::AdminSession() { return impl_->admin_session; }
RbacDao&         MysqlDaoFactory::Rbac()         { return impl_->rbac; }

}  // namespace nova

#endif  // ORMPP_ENABLE_MYSQL
