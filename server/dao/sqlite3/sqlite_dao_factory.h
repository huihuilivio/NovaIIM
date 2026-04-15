#pragma once

#include "../dao_factory.h"

#include <memory>
#include <string>

namespace nova {

/// SQLite 后端的 DaoFactory 实现
class SqliteDaoFactory : public DaoFactory {
public:
    explicit SqliteDaoFactory(const std::string& db_path);
    ~SqliteDaoFactory() override;

    UserDao&         User()         override;
    MessageDao&      Message()      override;
    AuditLogDao&     AuditLog()     override;
    AdminSessionDao& AdminSession() override;
    RbacDao&         Rbac()         override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nova
