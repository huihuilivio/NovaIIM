# Admin 模块实现计划

## 当前进度概览

| 组件 | 状态 | 备注 |
|------|------|------|
| l8w8jwt 库集成 | ✅ 完成 | FetchContent, HS256/384/512 |
| JwtUtils | ✅ 完成 | Sign / Verify, 可选算法 |
| AdminServer 框架 | ✅ 完成 | libhv HttpServer, JWT 中间件, X-Nova-* 防伪造 |
| ServerContext | ✅ 完成 | 原子计数器, AppConfig 按值存储(防悬垂引用) |
| http_helper | ✅ 完成 | JsonOk/JsonError, Pagination(int64_t Offset), HasPermission(精确分割匹配) |
| PasswordUtils | ✅ 完成 | PBKDF2-SHA256 (MbedTLS), 100k iterations, 全部 mbedtls 返回值检查 |
| DaoFactory 抽象工厂 | ✅ 完成 | 统一 DAO 访问入口，支持 SQLite/MySQL 后端切换 |
| SqliteDbManager (ormpp+SQLite3) | ✅ 完成 | WAL + FK + busy_timeout, PRAGMA 返回值检查 |
| MysqlDbManager (ormpp+MySQL) | ✅ 完成 | 连接池 + ConnGuard RAII, ping 健康检查, 自动重连 |
| MySQL 客户端库下载 | ✅ 完成 | Python 脚本自动检测/下载, cdn.mysql.com 多镜像回退 |
| Model 补全 | ✅ 完成 | User/UserDevice/Message/Conversation/ConversationMember/AuditLog/AdminSession/Role/Permission/RolePermission/UserRole, ormpp ADL 别名 |
| DAO 全部实现 | ✅ 完成 | UserDao/MessageDao/AuditLogDao/AdminSessionDao/RbacDao, 模板化支持双后端, 全参数化查询(防SQL注入), LIKE 通配符转义 |
| main.cpp 集成 | ✅ 完成 | CreateDaoFactory + ThreadPool 接入, 信号注册前置, JWT 密钥校验 |
| 线程安全 | ✅ 完成 | Connection::user_id_ atomic, device_id_ mutex, MPMCQueue move Push + 容量 assert, ThreadPool 重入安全 |
| AdminConfig | ✅ 完成 | jwt_secret + jwt_expires, server.yaml 已更新 |
| 鉴权中间件 | ✅ 完成 | JWT 验签 + X-Nova-* 清除 + 黑名单查询 + permissions 注入 |
| Handler 层 | ✅ 完成 | auth/dashboard/user/message/audit 全部 handler 已实现 |
| 审计日志写入 | ✅ 完成 | 全部写操作 handler 均集成 WriteAuditLog |
| DAO 目录重构 | ✅ 完成 | 公共接口 dao/, 模板实现 dao/impl/, 后端 dao/sqlite3/ + dao/mysql/ |

---

## 依赖关系

```
Phase 0 (基础工具)          ← ✅ 已完成
  └─ Phase 1 (DAO 层)       ← ✅ 已完成
       └─ Phase 2 (认证 + 鉴权) ← ✅ 已完成
            └─ Phase 3 (业务 API) ← ✅ 已完成
                 └─ Phase 4 (审计 + 测试) ← ⚠️ 审计已完成, 测试待补
```

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

---

## Phase 3 — 业务 API ✅

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
| 3.5 | DELETE /users/:id | SoftDelete → kick → 审计 | ✅ 完成 |
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

## Phase 4 — 审计日志 + 路由注册 + 测试

| # | 任务 | 说明 | 状态 |
|---|------|------|------|
| 4.1 | GET /audit-logs | AuditLogDao::List + 分页 + user_id/action/时间筛选 + operator_uid 缓存 | ✅ 完成 |
| 4.2 | 路由注册重构 | RegisterRoutes 注册全部路由(auth/dashboard/user/message/audit) | ✅ 完成 |
| 4.3 | AdminServer 注入 DAO | 构造函数接收 DaoFactory& 依赖 | ✅ 完成 |
| 4.4 | main.cpp 完整集成 | CreateDaoFactory → AdminServer 注入 | ✅ 完成 |
| 4.5 | JWT 单元测试 | Sign → Verify 往返 / 过期 / 篡改 | ❌ |
| 4.6 | 密码哈希测试 | Hash → Verify / 错误密码 | ❌ |
| 4.7 | DAO 单元测试 | 各 DAO 操作验证(SQLite 后端) | ❌ |
| 4.8 | Handler 测试 | mock DAO → 验证 HTTP 请求/响应 | ❌ |
| 4.9 | 集成测试 | login → 调用各 API → 验证审计 | ❌ |
| 4.10 | ConversationDao 实现 | 接口已定义, 需补模板实现 + 接入 DaoFactory | ❌ |

---

## 文件结构（当前实际）

```
server/
  main.cpp                          ← ✅ 入口: config → CreateDaoFactory → gateway → threadpool → admin
  CMakeLists.txt
  admin/
    admin_server.h / .cpp           ← ✅ 路由注册 + JWT中间件(黑名单+RBAC) + 全部Handler + 审计写入
    jwt_utils.h / .cpp              ← ✅ JWT 签发/验证 (l8w8jwt)
    password_utils.h / .cpp         ← ✅ PBKDF2-SHA256 (MbedTLS, 返回值全检查)
    http_helper.h                   ← ✅ JSON 响应 + 分页(int64_t) + 权限检查(精确匹配)
  core/
    app_config.h / .cpp             ← ✅ YAML 配置 (ylt struct_yaml), Config→AppConfig 已重命名
    server_context.h                ← ✅ 原子指标中心 (AppConfig 按值存储)
    logger.h / .cpp                 ← ✅ spdlog 封装
    formatters.h                    ← ✅ spdlog 自定义 formatter
    thread_pool.h                   ← ✅ Worker 线程池 (重入安全Stop, atomic exchange)
    mpmc_queue.h                    ← ✅ Vyukov MPMC (move Push, 容量 assert)
  dao/
    dao_factory.h / .cpp            ← ✅ DaoFactory 抽象工厂 + CreateDaoFactory 调度
    user_dao.h                      ← ✅ 抽象接口
    message_dao.h                   ← ✅ 抽象接口
    audit_log_dao.h                 ← ✅ 抽象接口
    admin_session_dao.h             ← ✅ 抽象接口
    rbac_dao.h                      ← ✅ 抽象接口
    conversation_dao.h              ← ⚠️ 接口已定义, 无模板实现, 未接入 DaoFactory
    impl/
      user_dao_impl.h / .cpp        ← ✅ 模板 XxxDaoImplT<DbMgr>, 双后端显式实例化
      message_dao_impl.h / .cpp     ← ✅ 同上
      audit_log_dao_impl.h / .cpp   ← ✅ 同上
      admin_session_dao_impl.h / .cpp ← ✅ 同上
      rbac_dao_impl.h / .cpp        ← ✅ 同上
    sqlite3/
      sqlite_db_manager.h / .cpp    ← ✅ ormpp + SQLite3 (WAL + FK + PRAGMA 检查)
      sqlite_dao_factory.h / .cpp   ← ✅ SqliteDaoFactory (Pimpl, 持有所有 DAO)
    mysql/
      mysql_db_manager.h / .cpp     ← ✅ ormpp + MySQL (连接池 + ConnGuard + 自动重连)
      mysql_dao_factory.h / .cpp    ← ✅ MysqlDaoFactory (Pimpl, 持有所有 DAO)
  model/
    types.h                         ← ✅ 全部 model + ormpp ADL 别名
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

## 已完成的安全加固

以下安全问题在 code review 中发现并已修复：

| 类别 | 修复内容 |
|------|---------|
| SQL 注入 | 全部 DAO 使用 ormpp query_s/update_some 参数绑定 |
| 请求头伪造 | AuthMiddleware 入口清除 X-Nova-User-Id / X-Nova-Permissions |
| UID 欺骗 | Heartbeat 使用 conn->user_id() 替代 pkt.uid |
| 数据竞争 | Connection user_id_ = atomic, device_id_ = mutex |
| 线程池 | Submit → worker_pool, Stop 重入安全, 信号注册前置 |
| 密码安全 | PBKDF2 100k iterations, mbedtls 返回值全检查 |
| 配置安全 | JWT 密钥启动校验 (默认值警告 + 长度检查) |
| LIKE 注入 | 通配符 %/_/\\ 转义 + ESCAPE 子句 |
| 整数溢出 | Pagination::Offset() → int64_t |

---

## 实施顺序

1. ~~**Phase 0** (0.1–0.5) — 工具层~~ ✅
2. ~~**Phase 1A** (1.1–1.5) — Model 补全~~ ✅
3. ~~**Phase 1B** (1.6–1.11) — DAO 实现 (ormpp)~~ ✅
4. **Phase 2** (2.2–2.6) — 认证 + 鉴权 ← **当前**
5. **Phase 3** (3.1–3.11) — 业务 API
6. **Phase 4** (4.1–4.8) — 收尾 + 测试

**关键里程碑：**
- ~~M1: DbManager + UserDao 具体实现 → 能执行 SQL~~ ✅
- M2: JWT 中间件 + /auth/login → 能登录获取 token
- M3: 全部 P0 API 可用 → 可交付前端对接
