# Admin 模块实现计划

## 当前进度概览

| 组件 | 状态 | 备注 |
|------|------|------|
| l8w8jwt 库集成 | ✅ 完成 | FetchContent, HS256/384/512 |
| JwtUtils | ✅ 完成 | Sign / Verify, 可选算法 |
| AdminServer 框架 | ✅ 完成 | libhv HttpServer, GET /healthz, GET /api/v1/stats, POST /api/v1/kick |
| ServerContext | ✅ 完成 | 原子计数器: connections, online_users, msgs, bad_packets, uptime |
| DAO 接口 | ⚠️ 部分 | UserDao / MessageDao / ConversationDao 抽象接口已定义，无具体实现 |
| Model | ⚠️ 部分 | User / Message 基础字段，缺少 password_hash / created_at 等 |
| AdminConfig | ⚠️ 待改 | 当前是 Bearer token 模式，需改为 jwt_secret |
| DbManager (SQLite3) | ❌ | 所有 DAO 的前置，基于 sqlite3 C API 封装 |
| 鉴权中间件 | ❌ | 当前是简单 token 比对，需重写为 JWT + RBAC |
| Handler 层 | ❌ | 无 auth/user/message/audit handler |
| 审计日志 | ❌ | 无 AuditLog model / dao / handler |

---

## 依赖关系

```
Phase 0 (基础工具)
  └─ Phase 1 (DAO 层)
       └─ Phase 2 (认证 + 鉴权)
            └─ Phase 3 (业务 API)
                 └─ Phase 4 (审计 + 测试)
```

---

## Phase 0 — 基础工具 + 响应格式

**目标：** 统一 HTTP 响应格式，准备密码哈希工具。

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 0.1 | 统一 JSON 响应助手 | `server/admin/http_helper.h` | `JsonOk(data)`, `JsonError(code, msg)` → `{"code":0,"msg":"ok","data":{}}` | ❌ |
| 0.2 | 分页请求解析 | `server/admin/http_helper.h` | `ParsePagination(req) → {page, page_size}`，校验范围 [1,100] | ❌ |
| 0.3 | 密码哈希工具 | `server/admin/password_utils.h/cpp` | 基于 MbedTLS（l8w8jwt 已引入）的 PBKDF2-SHA256 或直接 bcrypt | ❌ |
| 0.4 | AdminConfig 扩展 | `server/core/config.h` | `token` → `jwt_secret`，新增 `jwt_expires`(秒) | ❌ |
| 0.5 | server.yaml 更新 | `configs/server.yaml` | admin 段改为 `jwt_secret` / `jwt_expires` | ❌ |

---

## Phase 1 — Model 补全 + DAO 层

**目标：** 让 admin 模块能读写数据库。

### 1A — Model 补全

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 1.1 | User model 补全 | `server/model/types.h` | 添加 `password_hash`, `created_at`, `updated_at` | ❌ |
| 1.2 | UserDevice model | `server/model/types.h` | 添加 `UserDevice{id, user_id, device_id, device_type, last_active_at}` | ❌ |
| 1.3 | AuditLog model | `server/model/types.h` | `AuditLog{id, user_id, action, target_type, target_id, detail(json str), ip, created_at}` | ❌ |
| 1.4 | AdminSession model | `server/model/types.h` | `AdminSession{id, user_id, token_hash, expires_at, revoked}` | ❌ |

### 1B — DAO 实现

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 1.5 | DbManager (SQLite3) | `server/dao/db_manager.h/cpp` | sqlite3 C API 封装：Open/Close/Execute/Query，WAL 模式，从 DatabaseConfig.path 初始化 | ❌ |
| 1.6 | UserDao 补全 | `server/dao/user_dao.h/cpp` | 现有接口添加: `FindById`, `ListUsers(keyword, status, page, page_size)`, `Insert`, `UpdateStatus`, `UpdatePassword`, `SoftDelete`；添加具体实现类 `UserDaoImpl` (SQLite3) | ⚠️ 接口部分存在 |
| 1.7 | UserDeviceDao | `server/dao/user_device_dao.h/cpp` | `GetDevicesByUser(user_id)` | ❌ |
| 1.8 | MessageDao 补全 | `server/dao/message_dao.h/cpp` | 添加 `ListMessages(conv_id, start_time, end_time, page, page_size)` 分页查询；添加具体实现类 | ⚠️ 接口部分存在 |
| 1.9 | AuditLogDao | `server/dao/audit_log_dao.h/cpp` | `Insert(AuditLog)`, `List(user_id, action, start, end, page, page_size)` | ❌ |
| 1.10 | AdminSessionDao | `server/dao/admin_session_dao.h/cpp` | `Insert`, `IsRevoked(token_hash) → bool`, `RevokeByUser(user_id)` | ❌ |
| 1.11 | RbacDao | `server/dao/rbac_dao.h/cpp` | `GetUserPermissions(user_id) → vector<string>`, `HasPermission(user_id, code) → bool` | ❌ |
| 1.12 | main.cpp 集成 DbManager | `server/main.cpp` | 启动时 DbManager::Open(cfg.db.path)，关闭时 Close | ❌ |

---

## Phase 2 — 认证 + 鉴权

**目标：** JWT 登录、RBAC 中间件替换现有 Bearer token。

**前置依赖：** Phase 0 (密码哈希 + AdminConfig), Phase 1 (UserDao + RbacDao + AdminSessionDao)

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 2.1 | JWT 工具 | `server/admin/jwt_utils.h/cpp` | Sign / Verify (HS256) | ✅ 完成 |
| 2.2 | AuthMiddleware 重写 | `server/admin/admin_server.cpp` | 解析 JWT → 查 admin_sessions 黑名单 → 注入 user_id + permissions 到 req context (`req->SetContextPtr` 或自定义 header) | ❌ |
| 2.3 | PermissionGuard | `server/admin/http_helper.h` | `RequirePermission(req, "user.view")` → 从 req context 读权限列表，失败返回 403 JSON | ❌ |
| 2.4 | POST /auth/login | `server/admin/handlers/auth_handler.h/cpp` | 校验密码(PasswordUtils) → 检查 admin.login 权限(RbacDao) → 签发 JWT(JwtUtils) → 写 admin_sessions → 写 audit_logs | ❌ |
| 2.5 | POST /auth/logout | 同上 | 吊销当前 token(AdminSessionDao::Revoke) → 写 audit_logs | ❌ |
| 2.6 | GET /auth/me | 同上 | 从 req context 取 user_id → UserDao::FindById + RbacDao::GetUserPermissions | ❌ |

---

## Phase 3 — 业务 API

**目标：** 实现用户管理、消息管理、仪表盘增强。

**前置依赖：** Phase 2 (鉴权中间件可用)

### 3A — 仪表盘

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 3.1 | GET /api/v1/dashboard/stats | `server/admin/handlers/dashboard_handler.h/cpp` | 迁移现有 HandleStats，统一为 `{"code":0,"data":{...}}` 格式，权限 `admin.dashboard` | ⚠️ 逻辑已有，需迁移格式 |

### 3B — 用户管理

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 3.2 | GET /api/v1/users | `server/admin/handlers/user_handler.h/cpp` | UserDao::ListUsers，权限 `user.view` | ❌ |
| 3.3 | POST /api/v1/users | 同上 | UserDao::Insert + PasswordUtils::Hash，权限 `user.create`，审计 | ❌ |
| 3.4 | GET /api/v1/users/:id | 同上 | UserDao::FindById + UserDeviceDao + ConnManager::IsOnline，权限 `user.view` | ❌ |
| 3.5 | DELETE /api/v1/users/:id | 同上 | UserDao::SoftDelete → kick → 审计，权限 `user.delete` | ❌ |
| 3.6 | POST /api/v1/users/:id/reset-password | 同上 | UserDao::UpdatePassword + PasswordUtils::Hash → 审计，权限 `user.edit` | ❌ |
| 3.7 | POST /api/v1/users/:id/ban | 同上 | UserDao::UpdateStatus(2) → kick → 审计，权限 `user.ban` | ❌ |
| 3.8 | POST /api/v1/users/:id/unban | 同上 | UserDao::UpdateStatus(1) → 审计，权限 `user.ban` | ❌ |
| 3.9 | POST /api/v1/users/:id/kick | 同上 | ConnManager::GetConns → Close → 审计，权限 `user.ban` | ⚠️ 逻辑已有，需迁移 |

### 3C — 消息管理

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 3.10 | GET /api/v1/messages | `server/admin/handlers/message_handler.h/cpp` | MessageDao::ListMessages，权限 `msg.delete_all` | ❌ |
| 3.11 | POST /api/v1/messages/:id/recall | 同上 | MessageDao::UpdateStatus(1) → 审计，权限 `msg.delete_all` | ❌ |

---

## Phase 4 — 审计日志 + 路由注册 + 测试

| # | 任务 | 文件 | 说明 | 状态 |
|---|------|------|------|------|
| 4.1 | GET /api/v1/audit-logs | `server/admin/handlers/audit_handler.h/cpp` | AuditLogDao::List，权限 `admin.audit` | ❌ |
| 4.2 | 路由注册重构 | `server/admin/admin_server.cpp` | RegisterRoutes 中注册所有路由，替换现有 3 条路由 | ❌ |
| 4.3 | AdminServer 构造重构 | `server/admin/admin_server.h` | 注入 DAO 层依赖（DbManager 或具体 DAO 指针），去除 Options.token | ❌ |
| 4.4 | main.cpp 完整集成 | `server/main.cpp` | DbManager 初始化 → DAO 创建 → AdminServer 注入 | ❌ |
| 4.5 | JWT 单元测试 | `server/test/jwt_utils_test.cpp` | Sign → Verify 往返、过期、篡改、算法切换 | ❌ |
| 4.6 | 密码哈希测试 | `server/test/password_utils_test.cpp` | Hash → Verify 往返、错误密码 | ❌ |
| 4.7 | Handler 测试 | `server/test/admin_handler_test.cpp` | mock DAO，验证 HTTP 请求/响应 | ❌ |
| 4.8 | 集成测试 | `server/test/admin_api_test.cpp` | 启动 AdminServer → login → 调用各 API → 验证审计日志 | ❌ |

---

## 文件结构预览

```
server/
  admin/
    admin_server.h / .cpp           ← 路由注册 + 中间件（重构）
    jwt_utils.h / .cpp              ← ✅ JWT 签发/验证
    password_utils.h / .cpp         ← 密码哈希 (PBKDF2)
    http_helper.h                   ← JSON 响应助手 + 分页 + 权限检查
    handlers/
      auth_handler.h / .cpp         ← /auth/*
      dashboard_handler.h / .cpp    ← /dashboard/*
      user_handler.h / .cpp         ← /users/*
      message_handler.h / .cpp      ← /messages/*
      audit_handler.h / .cpp        ← /audit-logs
  dao/
    db_manager.h / .cpp             ← SQLite3 封装 (替代 ormpp)
    user_dao.h / .cpp               ← ⚠️ 接口存在，需补全 + 实现
    user_device_dao.h / .cpp        ← 用户设备查询
    message_dao.h / .cpp            ← ⚠️ 接口存在，需补全 + 实现
    conversation_dao.h / .cpp       ← ⚠️ 接口存在
    audit_log_dao.h / .cpp          ← 新增
    admin_session_dao.h / .cpp      ← 新增
    rbac_dao.h / .cpp               ← 新增
  model/
    types.h                         ← ⚠️ 补全字段 + 新增 model
    packet.h                        ← ✅ 已完成
  test/
    jwt_utils_test.cpp
    password_utils_test.cpp
    admin_handler_test.cpp
    admin_api_test.cpp
```

---

## 实施顺序

1. **Phase 0** (0.1–0.5) — 工具层，无外部依赖，可立即开始
2. **Phase 1A** (1.1–1.4) — Model 补全，可与 Phase 0 并行
3. **Phase 1B.5** (DbManager) — 先跑通连接池
4. **Phase 1B.6–1.12** — DAO 实现，依赖 DbManager
5. **Phase 2** — 认证，依赖 Phase 0 + Phase 1
6. **Phase 3** — 业务 API，依赖 Phase 2 的中间件
7. **Phase 4** — 收尾 + 测试

**并行化机会：**
- Phase 0 + Phase 1A 可并行
- Phase 1B 中各 DAO 之间无依赖，可并行开发
- Phase 3A/3B/3C 可在中间件就绪后并行

**关键里程碑：**
- M1: DbManager (SQLite3) + UserDao 具体实现 → 能执行 SQL
- M2: JWT 中间件 + /auth/login → 能登录获取 token
- M3: 全部 P0 API 可用 → 可交付前端对接
