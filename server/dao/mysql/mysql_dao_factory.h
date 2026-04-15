#pragma once

#ifdef ORMPP_ENABLE_MYSQL

#include "../dao_factory.h"

#include <memory>

namespace nova {

struct DatabaseConfig;

/// MySQL 后端的 DaoFactory 实现
class MysqlDaoFactory : public DaoFactory {
public:
    explicit MysqlDaoFactory(const DatabaseConfig& config);
    ~MysqlDaoFactory() override;

    UserDao&         User()         override;
    MessageDao&      Message()      override;
    AuditLogDao&     AuditLog()     override;
    AdminSessionDao& AdminSession() override;
    RbacDao&         Rbac()         override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova

#endif  // ORMPP_ENABLE_MYSQL
