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

    UserDao& User() override;
    MessageDao& Message() override;
    ConversationDao& Conversation() override;
    AuditLogDao& AuditLog() override;
    AdminSessionDao& AdminSession() override;
    AdminAccountDao& AdminAccount() override;
    RbacDao& Rbac() override;
    FileDao& File() override;
    FriendDao& Friend() override;
    GroupDao& Group() override;

    /// SQLite 单连接：Session() 返回持有互斥锁的 RAII 对象，
    /// 保证同一时刻只有一个线程访问 DB
    std::unique_ptr<DaoScopedConn> Session() override;

    bool BeginTransaction() override;
    bool Commit() override;
    bool Rollback() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova
