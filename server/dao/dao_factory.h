#pragma once

#include <memory>

namespace nova {

class UserDao;
class MessageDao;
class AuditLogDao;
class AdminSessionDao;
class AdminAccountDao;
class RbacDao;
struct DatabaseConfig;

/// DAO 工厂：抽象接口，对外提供统一 DAO 访问入口
/// 具体实现由 CreateDaoFactory 根据配置创建（SQLite / MySQL）
class DaoFactory {
public:
    virtual ~DaoFactory() = default;

    virtual UserDao&         User()         = 0;
    virtual MessageDao&      Message()      = 0;
    virtual AuditLogDao&     AuditLog()     = 0;
    virtual AdminSessionDao& AdminSession() = 0;
    virtual AdminAccountDao& AdminAccount() = 0;
    virtual RbacDao&         Rbac()         = 0;
};

/// 根据数据库配置创建对应的 DaoFactory 实例
/// 内部管理数据库连接生命周期（Open / InitSchema / Close）
/// @throws std::runtime_error 如果连接或建表失败
std::unique_ptr<DaoFactory> CreateDaoFactory(const DatabaseConfig& config);

} // namespace nova
