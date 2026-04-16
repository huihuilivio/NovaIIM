#pragma once

#include <memory>

namespace nova {

class UserDao;
class MessageDao;
class ConversationDao;
class AuditLogDao;
class AdminSessionDao;
class AdminAccountDao;
class RbacDao;
struct DatabaseConfig;

/// RAII 会话：将一条 DB 连接固定到当前线程
/// 作用域内所有 DAO 调用复用同一连接，避免线程内持有多条连接
/// 对 SQLite 为空操作（单连接）；对 MySQL 从连接池取出一条并固定
struct DaoScopedConn {
    virtual ~DaoScopedConn()                       = default;
    DaoScopedConn()                                = default;
    DaoScopedConn(const DaoScopedConn&)            = delete;
    DaoScopedConn& operator=(const DaoScopedConn&) = delete;
};

/// DAO 工厂：抽象接口，对外提供统一 DAO 访问入口
/// 具体实现由 CreateDaoFactory 根据配置创建（SQLite / MySQL）
class DaoFactory {
public:
    virtual ~DaoFactory() = default;

    virtual UserDao& User()                 = 0;
    virtual MessageDao& Message()           = 0;
    virtual ConversationDao& Conversation() = 0;
    virtual AuditLogDao& AuditLog()         = 0;
    virtual AdminSessionDao& AdminSession() = 0;
    virtual AdminAccountDao& AdminAccount() = 0;
    virtual RbacDao& Rbac()                 = 0;

    /// 开启会话：当前作用域内所有 DAO 操作复用同一 DB 连接
    /// 用法: auto session = ctx_.dao().Session();
    ///       ctx_.dao().User().FindByUid(...);  // 复用 session 连接
    /// 支持嵌套（内层为空操作），线程安全
    virtual std::unique_ptr<DaoScopedConn> Session() { return nullptr; }
};

/// 根据数据库配置创建对应的 DaoFactory 实例
/// 内部管理数据库连接生命周期（Open / InitSchema / Close）
/// @throws std::runtime_error 如果连接或建表失败
std::unique_ptr<DaoFactory> CreateDaoFactory(const DatabaseConfig& config);

}  // namespace nova
