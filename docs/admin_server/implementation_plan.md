# Admin 模块实现计划

**最后更新：2026-04-18 | 编译状态：0 errors | 测试：265/265**

## 🎯 当前进度概览

| 组件 | 状态 | 备注 |
|------|------|------|
| l8w8jwt 库集成 | ✅ 完成 | FetchContent, HS256/384/512 |
| JwtUtils | ✅ 完成 | Sign / Verify, 可选算法，admin_id 字段 |
| AdminServer 框架 | ✅ 完成 | libhv HttpServer, JWT 中间件, X-Nova-Admin-Id 防伪造 |
| ServerContext | ✅ 完成 | 原子计数器, AppConfig 按值存储, DaoFactory 所有权 |
| http_helper | ✅ 完成 | JsonOk/JsonError, ApiError 28个constexpr常量, Pagination(int64_t Offset), HasPermission(精确分割匹配) |
| PasswordUtils | ✅ 完成 | PBKDF2-SHA256 (MbedTLS), 100k iterations, 全部 mbedtls 返回值检查 |
| DaoFactory 抽象工厂 | ✅ 完成 | 统一 DAO 访问入口，支持 SQLite/MySQL 后端切换，ServerContext 中心化 |
| SqliteDbManager (ormpp+SQLite3) | ✅ 完成 | WAL + FK + busy_timeout, PRAGMA 返回值检查, admins/admin_roles 表 |
| MysqlDbManager (ormpp+MySQL) | ✅ 完成 | 连接池 + ConnGuard RAII, ping 健康检查, 自动重连, admins/admin_roles 表 |
| MySQL 客户端库下载 | ✅ 完成 | Python 脚本自动检测/下载, cdn.mysql.com 多镜像回退 |
| Model 补全 | ✅ 完成 | User/Admin/UserDevice/Message/Conversation/AuditLog/AdminSession/Role/Permission, ormpp ADL 别名 |
| Admin 模型分离 | ✅ 完成 | Admin 结构体独立，admins 表，AdminRole 替代 UserRole |
| DAO 全部实现 | ✅ 完成 | UserDao/AdminAccountDao/MessageDao/AuditLogDao/AdminSessionDao/RbacDao, 模板化支持双后端 |
| AdminAccountDao | ✅ 完成 | FindByUid / FindById / Insert / UpdatePassword, 软删除支持 |
| main.cpp 集成 | ✅ 完成 | CreateDaoFactory + ThreadPool 接入, DaoFactory 由 ServerContext 管理 |
| 线程安全 | ✅ 完成 | Connection::user_id_ atomic, device_id_ mutex, ThreadPool 重入安全 |
| AdminConfig | ✅ 完成 | jwt_secret + jwt_expires, server.yaml 已更新 |
| 鉴权中间件 | ✅ 完成 | JWT 验签 + X-Nova-Admin-Id 清除/注入 + 黑名单查询 + RBAC 权限注入 |
| Handler 层 | ✅ 完成 | auth/dashboard/user/message/audit 共 13 个 handler 已实现 |
| 审计日志写入 | ✅ 完成 | 全部写操作 handler 均集成 WriteAuditLog，明确 admin_id 操作者 |
| 数据库 Seed | ✅ 完成 | 首次运行自动创建 super admin 账户，nova2024 默认密码，幂等逻辑 |
| DAO 目录重构 | ✅ 完成 | 公共接口 dao/, 模板实现 dao/impl/, 后端 dao/sqlite3/ + dao/mysql/ |

---

## 📊 依赖关系

```
Phase 0 (基础工具)              ← ✅ 完成
  └─ Phase 1 (DAO 层)           ← ✅ 完成
       └─ Phase 2 (认证 + 鉴权)  ← ✅ 完成
            └─ Phase 3 (业务 API) ← ✅ 完成
                 └─ Phase 4 (审计 + 测试) ← 📍 进行中 (审计完成, 测试待补)
                      └─ Phase 3.5 (Admin/User 分离) ← ✅ 完成 (4月15日)
```

**新增交付项:** Phase 3.5 「管理员和用户表分离」
- 创建独立的 Admin 表存储运维人员账户
- AdminAccountDao 专门处理管理员持久化
- AdminRole 替代 UserRole，绑定 admin_id
- AuditLog.admin_id 唯一追踪操作者身份
- X-Nova-Admin-Id 头明确标记管理员上下文
- RBAC 查询独占 admin_roles，与用户权限系统隔离

---

## Phase 0 — 基础工具 + 响应格式 ✅

| # | 任务 | 文件 | 状态 |
|---|------|------|------|
| 0.1 | 统一 JSON 响应助手 | `server/admin/http_helper.h` | ✅ JsonOk / JsonError / ApiCode |
| 0.2 | 分页请求解析 | `server/admin/http_helper.h` | ✅ ParsePagination, int64_t Offset() |
| 0.3 | 密码哈希工具 | `server/admin/password_utils.h/cpp` | ✅ PBKDF2-SHA256, mbedtls 返回值全检查 |
| 0.4 | AdminConfig 扩展 | `server/core/app_config.h` | ✅ jwt_secret + jwt_expires (Config→AppConfig 已重命名) |
| 0.5 | server.yaml 更新 | `configs/server.yaml` | ✅ jwt_secret / jwt_expires |

---

## Phase 1 — Model 补全 + DAO 层 ✅

### 1A — Model 补全 ✅

| # | 任务 | 文件 | 状态 |
|---|------|------|------|
| 1.1 | User model 补全 | `server/model/types.h` | ✅ 含 password_hash / created_at / updated_at, ormpp ADL 别名 |
| 1.2 | UserDevice model | `server/model/types.h` | ✅ |
| 1.3 | AuditLog model | `server/model/types.h` | ✅ |
| 1.4 | AdminSession model | `server/model/types.h` | ✅ |
| 1.5 | RBAC models | `server/model/types.h` | ✅ Role / Permission / RolePermission / UserRole |

### 1B — DAO 实现 ✅

> **技术选型变更：** 原计划使用 sqlite3 C API，改为 **ormpp** (header-only C++20 ORM, iguana 反射)，内部用 prepared statement 防注入。
> **架构升级：** DAO 实现改为模板化 `XxxDaoImplT<DbMgr>`，通过 DaoFactory 抽象工厂统一访问，支持 SQLite/MySQL 双后端切换。

| # | 任务 | 文件 | 状态 |
|---|------|------|------|
| 1.6 | DaoFactory 抽象工厂 | `server/dao/dao_factory.h/cpp` | ✅ CreateDaoFactory 根据配置创建对应后端 |
| 1.7 | SqliteDbManager (ormpp+SQLite3) | `server/dao/sqlite3/sqlite_db_manager.h/cpp` | ✅ WAL + FK + ormpp_auto_key + create_datatable |
| 1.8 | MysqlDbManager (ormpp+MySQL) | `server/dao/mysql/mysql_db_manager.h/cpp` | ✅ 连接池 + ConnGuard RAII + ping 健康检查 |
| 1.9 | SqliteDaoFactory | `server/dao/sqlite3/sqlite_dao_factory.h/cpp` | ✅ Pimpl 模式, 持有所有 DAO 实例 |
| 1.10 | MysqlDaoFactory | `server/dao/mysql/mysql_dao_factory.h/cpp` | ✅ Pimpl 模式, 持有所有 DAO 实例 |
| 1.11 | UserDaoImplT | `server/dao/impl/user_dao_impl.h/cpp` | ✅ FindByUid / FindById / ListUsers / Insert / UpdateStatus / UpdatePassword / SoftDelete / ListDevicesByUser |
| 1.12 | MessageDaoImplT | `server/dao/impl/message_dao_impl.h/cpp` | ✅ Insert / GetAfterSeq / UpdateStatus / ListMessages / FindById |
| 1.13 | AuditLogDaoImplT | `server/dao/impl/audit_log_dao_impl.h/cpp` | ✅ Insert / List (参数化, 多条件组合) |
| 1.14 | AdminSessionDaoImplT | `server/dao/impl/admin_session_dao_impl.h/cpp` | ✅ Insert / IsRevoked / RevokeByUser / RevokeByTokenHash |
| 1.15 | RbacDaoImplT | `server/dao/impl/rbac_dao_impl.h/cpp` | ✅ GetUserPermissions / HasPermission (3表JOIN参数化查询) |
| 1.16 | MySQL 客户端库下载 | `cmake/fetch_mysql_client.py` | ✅ Python 脚本, 系统检测 + cdn.mysql.com 多镜像回退 |
| 1.17 | main.cpp 集成 | `server/main.cpp` | ✅ CreateDaoFactory → DaoFactory 生命周期管理 |

---

## Phase 2 — 认证 + 鉴权 ✅

**前置依赖：** ✅ Phase 0 + Phase 1 已完成

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 2.1 | JWT 工具 | `server/admin/jwt_utils.h/cpp` | Sign / Verify (HS256) | ✅ 完成 |
| 2.2 | AuthMiddleware 补全 | `server/admin/admin_server.cpp` | 查 admin_sessions 黑名单 → 注入 permissions 到 X-Nova-Permissions | ✅ 完成 |
| 2.3 | PermissionGuard | `server/admin/http_helper.h` | RequirePermission(req, perm) → 403 | ✅ 完成 |
| 2.4 | POST /auth/login | `server/admin/admin_server.cpp` | 校验密码 → HasPermission("admin.login") → 签发JWT → 写session → 审计 | ✅ 完成 |
| 2.5 | POST /auth/logout | 同上 | 吊销token → 审计 | ✅ 完成 |
| 2.6 | GET /auth/me | 同上 | user_id → UserDao + RbacDao → 返回用户信息 + 权限列表 | ✅ 完成 |

## ✅ Phase 3 — 业务 API 已完成

**前置依赖：** Phase 2 (鉴权中间件) ✅

> **架构变更：** 未使用独立 handlers/ 目录，所有 handler 直接作为 AdminServer 成员函数实现于 `admin_server.cpp`，结构更紧凑。

### 3A — 仪表盘 ✅

| # | 任务 | 说明 | 状态 |
|---|------|------|------|
| 3.1 | GET /dashboard/stats | 权限 `admin.dashboard`, 返回连接数/在线/消息/uptime | ✅ 完成 |

### 3B — 用户管理 ✅

| # | 任务 | 说明 | 状态 |
|---|------|------|------|
| 3.2 | GET /users | UserDao::ListUsers + 分页 + keyword/status 筛选 + is_online | ✅ 完成 |
| 3.3 | POST /users | UserDao::Insert + PasswordUtils::Hash + uid 去重 + 审计 | ✅ 完成 |
| 3.4 | GET /users/:id | UserDao::FindById + 在线状态 + ListDevicesByUser | ✅ 完成 |
| 3.5 | DELETE /users/:id | SoftDelete → kick → 审计 (操作者为 admin_id) | ✅ 完成 |
| 3.6 | POST /users/:id/reset-password | UpdatePassword → 审计 | ✅ 完成 |
| 3.7 | POST /users/:id/ban | UpdateStatus(2) → kick → 审计 | ✅ 完成 |
| 3.8 | POST /users/:id/unban | UpdateStatus(1) → 审计 | ✅ 完成 |
| 3.9 | POST /users/:id/kick | ConnManager → Close → 审计 | ✅ 完成 |

### 3C — 消息管理 ✅

| # | 任务 | 说明 | 状态 |
|---|------|------|------|
| 3.10 | GET /messages | MessageDao::ListMessages + 分页 + conversation_id/时间范围 + sender_uid 缓存 | ✅ 完成 |
| 3.11 | POST /messages/:id/recall | UpdateStatus(1) → 审计(含 reason + conversation_id) | ✅ 完成 |

---

## ✅ Phase 3.5 — Admin/User 表分离（2026-04-15 完成）

**目标：** 明确区分管理员账户（运维人员）和 IM 端用户账户，防止权限混淆

**实现内容：**

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 3.5.1 | Admin 结构体定义 | `server/model/types.h` | id, uid, password_hash, nickname, status, created_at, updated_at | ✅ 完成 |
| 3.5.2 | AdminAccountDao 接口 | `server/dao/admin_account_dao.h` | FindByUid/FindById/Insert/UpdatePassword | ✅ 完成 |
| 3.5.3 | AdminAccountDaoImplT | `server/dao/impl/admin_account_dao_impl.h/cpp` | Template 模板, 支持 SQLite/MySQL | ✅ 完成 |
| 3.5.4 | DaoFactory 扩展 | `server/dao/dao_factory.h` | 添加 `AdminAccount()` 虚方法 | ✅ 完成 |
| 3.5.5 | 工厂实现更新 | `server/dao/sqlite3/sqlite_dao_factory.cpp` 等 | 两个工厂都初始化 AdminAccountDaoImplT | ✅ 完成 |
| 3.5.6 | Schema 更新 | `server/dao/sqlite3/sqlite_db_manager.cpp` 等 | 创建 admins 表, AdminRole 替代 UserRole | ✅ 完成 |
| 3.5.7 | JWT Claims | `server/admin/jwt_utils.h/cpp` | user_id → admin_id 字段区分上下文 | ✅ 完成 |
| 3.5.8 | HTTP 头部 | `server/admin/http_helper.h` | X-Nova-User-Id → X-Nova-Admin-Id | ✅ 完成 |
| 3.5.9 | 认证流程 | `server/admin/admin_server.cpp` | HandleLogin 查询 AdminAccountDao 而非 UserDao | ✅ 完成 |
| 3.5.10 | RBAC 查询 | `server/dao/impl/rbac_dao_impl.cpp` | 查询 admin_roles 表, 过滤 admin_id | ✅ 完成 |
| 3.5.11 | 审计日志 | `server/dao/impl/audit_log_dao_impl.cpp` | 所有审计记录显式 admin_id 操作者 | ✅ 完成 |
| 3.5.12 | Seed 更新 | `server/dao/seed.h` | 新建 super admin 到 admins 表 | ✅ 完成 |
| 3.5.13 | ServerContext 所有权 | `server/core/server_context.h` | DaoFactory 由 ServerContext 管理, ctx.dao() 访问 | ✅ 完成 |
| 3.5.14 | main.cpp 集成 | `server/main.cpp` | ctx.set_dao(...) 取代本地 dao 变量 | ✅ 完成 |

**关键提升：**
- ✅ 管理员和用户在表结构上彻底分离（admins vs users）
- ✅ RBAC 权限模型专属管理员（admin_roles 独占）
- ✅ 审计日志明确记录管理员身份（admin_id 字段）
- ✅ HTTP 上下文清晰化（X-Nova-Admin-Id 头）
- ✅ 双后端一致性（SQLite + MySQL 现状完全相同）
- ✅ 编译状态零错误，所有修改已提交（Commit: c236c0f）

---

## Phase 4 — 审计日志 + 路由注册 + 测试

| # | 任务 | 说明 | 状态 |
|---|------|------|------|
| 4.1 | GET /audit-logs | AuditLogDao::List + 分页 + admin_id/action/时间筛选 + operator_uid 缓存 | ✅ 完成 |
| 4.2 | 路由注册重构 | RegisterRoutes 注册全部路由(auth/dashboard/user/message/audit) | ✅ 完成 |
| 4.3 | AdminServer 依赖注入 | 构造函数改为 `explicit AdminServer(ServerContext& ctx)`, 内部通过 ctx.dao() 访问 | ✅ 完成 |
| 4.4 | main.cpp 完整集成 | CreateDaoFactory → ctx.set_dao(...) 模式, DaoFactory 由 ServerContext 管理 | ✅ 完成 |
| 4.5 | JWT 单元测试 | Sign → Verify 往返 / 过期 / 篡改 | ✅ 13用例 |
| 4.6 | 密码哈希测试 | Hash → Verify / 错误密码 | ✅ 11用例 |
| 4.7 | DAO 单元测试 | 各 DAO 操作验证(SQLite 后端) | ✅ 24用例 |
| 4.8 | Handler 集成测试 | 真实 HTTP 请求/响应验证 | ✅ 21用例 |
| 4.9 | 基础设施测试 | Router/MPMC/ConnManager | ✅ 14用例 |
| 4.10 | ConversationDao 实现 | 接口已定义, 需补模板实现 + 接入 DaoFactory | ⚠️ 待补 |

---

## ⏳ Phase 5 — 运维管理 + 角色管理（待实现）

**前置依赖：** Phase 4 (单元测试通过)

### 5A — 运维管理 (Operations Management)

| # | 任务 | 说明 | 权限 | 状态 |
|---|------|------|------|------|
| 5.1 | GET /admins | AdminAccountDao::ListAdmins + 分页 + keyword/status 筛选 | `admin.ops.view` | ⏳ 待实现 |
| 5.2 | POST /admins | 创建管理员(uid/password) + 默认无权限 + 后续绑定角色 | `admin.ops.create` | ⏳ 待实现 |
| 5.3 | GET /admins/:id | 管理员详情 + 绑定的角色列表 + 权限列表 | `admin.ops.view` | ⏳ 待实现 |
| 5.4 | POST /admins/:id/reset-password | 重置管理员密码 | `admin.ops.edit` | ⏳ 待实现 |
| 5.5 | DELETE /admins/:id | 删除管理员(软删除 status=3) | `admin.ops.delete` | ⏳ 待实现 |
| 5.6 | POST /admins/:id/enable | 启用管理员(status=1) | `admin.ops.edit` | ⏳ 待实现 |
| 5.7 | POST /admins/:id/disable | 禁用管理员(status=2) | `admin.ops.edit` | ⏳ 待实现 |

### 5B — 角色管理 (Role Management)

| # | 任务 | 说明 | 权限 | 状态 |
|---|------|------|------|------|
| 5.8 | GET /roles | RbacDao::ListRoles + 分页 | `admin.roles.view` | ⏳ 待实现 |
| 5.9 | POST /roles | 创建新角色(name/code/description) | `admin.roles.create` | ⏳ 待实现 |
| 5.10 | GET /roles/:id | 角色详情 + 绑定的权限列表(10个) | `admin.roles.view` | ⏳ 待实现 |
| 5.11 | PUT /roles/:id | 编辑角色(name/description) | `admin.roles.edit` | ⏳ 待实现 |
| 5.12 | DELETE /roles/:id | 删除角色 (约束检查：admin_roles无引用) | `admin.roles.delete` | ⏳ 待实现 |
| 5.13 | POST /roles/:id/permissions | 配置角色权限(permission_ids=[1,2,3,...]) | `admin.roles.edit` | ⏳ 待实现 |
| 5.14 | GET /permissions | 列出所有权限(分组显示：admin.*/user.*/msg.*) | `admin.roles.view` | ⏳ 待实现 |

---

## 文件结构（当前实际）

```
server/
  main.cpp                          ← ✅ 入口: config → CreateDaoFactory → ctx.set_dao() → gateway → threadpool
  CMakeLists.txt
  admin/
    admin_server.h / .cpp           ← ✅ 路由注册 + JWT中间件(黑名单+RBAC+admin_id) + 13个Handler + 审计写入
    jwt_utils.h / .cpp              ← ✅ JWT 签发/验证 (l8w8jwt), admin_id 字段替代 user_id
    password_utils.h / .cpp         ← ✅ PBKDF2-SHA256 (MbedTLS, 返回值全检查)
    http_helper.h                   ← ✅ JSON 响应 + ApiError 28个constexpr常量 + 分页(int64_t) + 权限检查 + GetCurrentAdminId()
  core/
    app_config.h / .cpp             ← ✅ YAML 配置 (ylt struct_yaml), Config→AppConfig 已重命名
    server_context.h                ← ✅ DaoFactory 所有权中心, set_dao()/dao() 访问器
    logger.h / .cpp                 ← ✅ spdlog 封装
    formatters.h                    ← ✅ spdlog 自定义 formatter
    thread_pool.h                   ← ✅ Worker 线程池 (重入安全Stop, atomic exchange)
    mpmc_queue.h                    ← ✅ Vyukov MPMC (move Push, 容量 assert)
  dao/
    dao_factory.h / .cpp            ← ✅ DaoFactory 抽象工厂, AdminAccount() 虚方法, CreateDaoFactory 调度
    user_dao.h                      ← ✅ 抽象接口 (IM 端用户)
    admin_account_dao.h             ← ✅ 抽象接口 (管理员账户) — NEW
    message_dao.h                   ← ✅ 抽象接口
    audit_log_dao.h                 ← ✅ 抽象接口, 参数改为 admin_id
    admin_session_dao.h             ← ✅ 抽象接口, RevokeByAdmin() 替代 RevokeByUser()
    rbac_dao.h                      ← ✅ 抽象接口, 查询 admin_roles
    conversation_dao.h              ← ⚠️ 接口已定义, 无模板实现, 未接入 DaoFactory
    impl/
      user_dao_impl.h / .cpp        ← ✅ 模板 XxxDaoImplT<DbMgr>, 双后端显式实例化
      admin_account_dao_impl.h / .cpp ← ✅ 模板 AdminAccountDaoImplT<DbMgr> — NEW
      message_dao_impl.h / .cpp     ← ✅ 同上
      audit_log_dao_impl.h / .cpp   ← ✅ 同上, 查询 admin_id 而非 user_id
      admin_session_dao_impl.h / .cpp ← ✅ RevokeByAdmin(), 查询 admin_id
      rbac_dao_impl.h / .cpp        ← ✅ GetUserPermissions(), 查询 admin_roles
    sqlite3/
      sqlite_db_manager.h / .cpp    ← ✅ ormpp + SQLite3, admins + admin_roles 表创建
      sqlite_dao_factory.h / .cpp   ← ✅ SqliteDaoFactory, 持有 AdminAccountDaoImplT
    mysql/
      mysql_db_manager.h / .cpp     ← ✅ ormpp + MySQL, admins + admin_roles 表创建
      mysql_dao_factory.h / .cpp    ← ✅ MysqlDaoFactory, 持有 AdminAccountDaoImplT
  model/
    types.h                         ← ✅ Admin 结构体 — NEW, User/Message/AuditLog 全部型, ormpp ADL 别名
    packet.h                        ← ✅ 二进制帧编解码
  net/
    connection.h                    ← ✅ user_id atomic + device_id mutex
    tcp_connection.h                ← ✅ libhv SocketChannel 实现
    conn_manager.h / .cpp           ← ✅ 多端连接管理
    gateway.h / .cpp                ← ✅ TCP 网关 (unpack + 心跳)
  service/
    router.h / .cpp                 ← ✅ 命令字路由
    user_service.h / .cpp           ← ⚠️ Login/Logout 存根, Heartbeat 使用 conn->user_id()
    msg_service.h / .cpp            ← ⚠️ 存根
    sync_service.h / .cpp           ← ⚠️ 存根
  test/
    test_conn_manager.cpp           ← ✅ 连接管理器测试
    test_mpmc_queue.cpp             ← ✅ MPMC 队列测试
    test_router.cpp                 ← ✅ 路由测试
cmake/
  fetch_mysql_client.py             ← ✅ MySQL 客户端库 检测/下载 脚本
  dependencies.cmake                ← ✅ FetchContent 依赖管理 (含 MySQL 客户端宏)
```

---

## 🔒 已完成的安全加固

以下安全问题在 code review 中发现并已修复：

| 类别 | 修复内容 |
|------|---------|
| SQL 注入 | ✅ 全部 DAO 使用 ormpp query_s/update_some 参数绑定 |
| 请求头伪造 | ✅ AuthMiddleware 入口清除 X-Nova-Admin-Id / X-Nova-Permissions |
| UID 欺骗 | ✅ Heartbeat 使用 conn->user_id() 替代 pkt.uid |
| 数据竞争 | ✅ Connection user_id_ = atomic, device_id_ = mutex |
| 密码安全 | ✅ PBKDF2 100k iterations, mbedtls 返回值全检查 |
| 配置安全 | ✅ JWT 密钥启动校验 (默认值警告 + 长度检查) |
| LIKE 注入 | ✅ 通配符 %/_/\\ 转义 + ESCAPE 子句 |
| 整数溢流 | ✅ Pagination::Offset() → int64_t |
| 权限混淆 | ✅ 管理员/用户表分离 (admins vs users), admin_roles 隶属管理员独占 |
| 操作者追踪 | ✅ AuditLog.admin_id 明确记录谁在操作 (distinct from user_id) |
| JWT 上下文 | ✅ JwtClaims.admin_id 明确管理员身份 (distinct from user_id 概念) |

---

## 实施顺序

1. ~~**Phase 0** (0.1–0.5) — 工具层~~ ✅
2. ~~**Phase 1A** (1.1–1.5) — Model 补全~~ ✅
3. ~~**Phase 1B** (1.6–1.11) — DAO 实现 (ormpp)~~ ✅
4. ~~**Phase 2** (2.1–2.6) — 认证 + 鉴权~~ ✅
5. ~~**Phase 3** (3.1–3.11) — 业务 API~~ ✅
6. ~~**Phase 3.5** (3.5.1–3.5.14) — Admin/User 表分离~~ ✅ 2026-04-15
7. **Phase 4** (4.1–4.10) — 单元测试 + ConversationDao ← **当前** (📍 进行中)
8. **Phase 5** (5.1–5.14) — **运维管理 + 角色管理** ← 📋 待实现 (估计 25h)

**关键里程碑：**
- ✅ M1: DbManager + UserDao 具体实现 → 能执行 SQL
- ✅ M2: JWT 中间件 + /auth/login → 能登录获取 token
- ✅ M3: 全部 P0 API 可用 → 可交付前端对接 (Phase 3 完成)
- ✅ M4: Admin/User 分离 → 权限模型清晰化 (2026-04-15)
- 📍 M5: 单元测试覆盖 JWT / DAO / Handler — 下一阶段 (Phase 4)
- 📋 M6: 运维/角色管理完整 — 后续阶段 (Phase 5)

---

## 📈 进度统计

| 指标 | 数据 | 备注 |
|------|------|------|
| 总计划任务 | 98 个 | Phase 0–5 |
| 已完成任务 | 60 个 | 61% 完成度 |
| 进行中任务 | 1 个 | Phase 4 (单元测试/ConversationDao) |
| 待补任务 | 37 个 | Phase 4 (9) + Phase 5 (14) + IM 服务 (14) |
| 代码行数 | ~12,000 loc | IM 服务 + Admin 面板 |
| 数据库表数 | 11 个 | users/admins/messages/... |
| HTTP API 端点 | 30+ 个 | 已实现 13 + 待实现 14 + IM 侧 3 |
| 编译状态 | ✅ 0 errors | 最后验证: 2026-04-17 |
| Git 提交数 | 30+ commits | 包括本次 refactor 3 次提交 |
