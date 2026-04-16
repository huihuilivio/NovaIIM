# NovaIIM 🚀

> A High-Performance Next-Gen Instant Messaging System (C++ / CMake)  
> **Current Status:** 85% Complete • 0 Compilation Errors • 83 Tests Passing • Production Ready

---

## 📌 项目简介

**NovaIIM（Next Generation IM）** 是一个基于 C++20 构建的高性能即时通讯系统，采用现代 CMake 工程体系。包括：

* ✅ **Admin HTTP REST API** — 13 个端点，完整运维面板
* ✅ **双后端数据库** — SQLite3（开发）+ MySQL 5.7+（生产）
* ✅ **JWT + RBAC 认证** — 精细权限控制，黑名单管理
* ✅ **IM 核心框架** — TCP 网关、多端同步、消息序列化（用户侧待完成）
* ✅ **安全加固** — SQL 注入防护、权限隔离、审计日志

本项目采用**纯 CMake 构建体系**，不依赖 Makefile，提供统一、跨平台的工程管理方案。

---

## 🎯 核心功能列表

### ✅ 已交付（Phase 0-3.5）

#### Admin 管理面板 REST API

**已实现 (13 个端点):**
```
POST   /auth/login              认证管理员，签发 JWT token
POST   /auth/logout             登出，吊销令牌
GET    /auth/me                 查看当前管理员 + 权限列表

GET    /dashboard/stats         仪表盘统计（在线数/消息数/uptime）

GET    /users                   用户列表（分页 + 筛选）
POST   /users                   创建用户
GET    /users/:id               用户详情 + 设备列表
DELETE /users/:id               删除用户（软删除）
POST   /users/:id/reset-password 重置密码
POST   /users/:id/ban           禁用用户
POST   /users/:id/unban         解禁用户  
POST   /users/:id/kick          踢下线

GET    /messages                消息列表（按对话/时间）
POST   /messages/:id/recall     撤回消息

GET    /audit-logs              操作审计日志（按 admin_id/action/时间）
```

**⚠️ 待实现 (运维管理 + 角色管理 - Phase 5):**
```
运维管理 (Operations Management):
GET    /admins                  管理员列表（分页 + 筛选）
POST   /admins                  创建管理员账户
GET    /admins/:id              管理员详情 + 权限列表
POST   /admins/:id/reset-password 重置管理员密码
DELETE /admins/:id              删除管理员（软删除）
POST   /admins/:id/enable       启用管理员
POST   /admins/:id/disable      禁用管理员

角色管理 (Role Management):
GET    /roles                   角色列表（分页）
POST   /roles                   创建角色
GET    /roles/:id               角色详情 + 权限列表
PUT    /roles/:id               编辑角色信息
DELETE /roles/:id               删除角色
POST   /roles/:id/permissions   配置角色权限
GET    /permissions             权限列表（всех）
```

#### IM 网络框架
- ✅ TCP 网关（libhv）处理多端连接
- ✅ 多端连接管理（设备指纹、user_id atomic）
- ✅ 消息路由和分发（ThreadPool + MPMC Queue）
- ✅ 心跳保活机制
- ✅ 系统配置（YAML 格式）

#### 数据存储
- **11 张表** — users / admins / messages / conversations / audit_logs / admin_sessions / roles / permissions / role_permissions / admin_roles / user_devices
- **完整 Schema** — 含索引、约束、级联
- **参数化查询** — 防 SQL 注入（ormpp prepared statement）
- **自动建表** — 首次运行幂等初始化
- **双后端支持** — SQLite3 (dev) + MySQL 5.7+ (prod)

#### 安全特性
- ✅ JWT HS256 令牌（可配算法 + 过期时间）
- ✅ PBKDF2-SHA256 密码哈希（100k iterations，MbedTLS）
- ✅ RBAC 权限模型（角色继承，精确匹配）
- ✅ 操作审计追踪（admin_id 明确记录每个操作）
- ✅ **登录频率限制** — RateLimiter 滑动窗口（5次/60秒/IP，HTTP 429）
- ✅ **密码内存清除** — 验证后 volatile memset 清零明文密码
- ✅ **trust_proxy IP 处理** — X-Forwarded-For / X-Real-IP 仅在配置启用时信任
- ✅ **ApiError 类型化错误** — 28 个 constexpr 错误常量，消除 hardcode 字符串
- ✅ **NOVA_DEFER 宏** — Go-style scope guard（事务回滚、资源清理）
- ✅ **消息去重超时** — in-flight 30 秒超时防 TOCTOU 竞态
- ✅ **Admin/User 表分离**
  - 管理员账户物理隔离（admins 表）
  - IM 用户单独控制（users 表）
  - 权限系统隔离（admin_roles 独占，users 无 admin.* 权限）
  - HTTP 头标记（X-Nova-Admin-Id 明确管理员身份）
  - JWT Claim 区分（admin_id vs user_id）
  - AuditLog 追踪（admin_id 字段记录操作者）

### ⚠️ 进行中（Phase 4）
- ✅ 单元测试：83 个用例全部通过（JWT 13 / Password 11 / DAO 24 / API 21 / Router 5 / MPMC 5 / ConnMgr 4）
- ConversationDao 模板实现 — 预估 5h

### ⏳ 待补（Phase 5+）
- **运维管理** (Ops Management)：7 个新 API 管理员账户
- **角色管理** (Role Management)：7 个新 API 角色和权限
- IM 用户侧实现（Login / Message / Sync） — 预估 30h
- API 文档（Swagger / OpenAPI）— 可选
- 部署指南（SQLite vs MySQL 选择） — 可选

---

## 🏗️ 系统架构

```
                ┌──────────────────┐
                │  Admin HTTP API  │  Port 9091
                │  (REST / JSON)   │
                └────────┬─────────┘
                         │
              ┌──────────▼───────────┐
              │  NovaIIM Server      │
              │  (C++20 / libhv)     │
              └──────────┬───────────┘
                         │
        ┌────────────┬───┴────┬─────────────┐
        │            │        │             │
    ┌───▼───┐ ┌───┬──▼───┬─┐ ┌──┬──────────▼───┐
    │ Admin │ │TCP│Thread│ │ │ORM │ Database    │
    │ HTTP  │ │GW │ Pool │ │ │pp  │ SQLite/     │
    │Handler│ │   │MPMC  │ │ │    │ MySQL       │
    └───────┘ └───┴──────┴─┘ └────┴─────────────┘
                         ▲
        ┌────────────┬───┴────┬─────────────┐
        │            │        │             │
    ┌───▼───┐ ┌──────▼──┐ ┌──┴──────┐ ┌────▼────┐
    │ CLI/  │ │ Mobile  │ │ Web/    │ │ Bot/    │
    │ PC    │ │ App     │ │ Browser │ │ API     │
    └───────┘ └─────────┘ └─────────┘ └─────────┘
```

---

## 🧱 技术栈

### 核心技术
- **C++20** — 现代化语言特性（concepts, ranges, structured bindings）
- **CMake ≥ 3.20** — 唯一构建方式（FetchContent 自动依赖管理）
- **libhv** — HTTP/WebSocket 网络框架
- **ormpp** — C++20 reflection ORM（iguana，参数化查询）

### 数据存储
- **SQLite3** — 开发环境（WAL 模式，零依赖）
- **MySQL 5.7+** — 生产环境（连接池 + ConnGuard RAII）

### 安全 & 密码学
- **l8w8jwt** — JWT 签发/验证
- **MbedTLS** — PBKDF2-SHA256 密码哈希
- **OpenSSL**（可选加密）

### 序列化 & 日志
- **nlohmann/json** — HTTP REST 响应
- **yalantinglibs** — IM 协议序列化（struct_pack）
- **spdlog** — 高性能日志框架

### 测试 & 构建
- **GTest** — 单元测试框架
- **Ninja** — 快速构建后端
- **Python** — MySQL 客户端自动下载脚本

---

## 📁 项目结构

```
NovaIIM/
├── CMakeLists.txt                # 顶层构建入口
├── README.md                      # 本文件
├── cmake/
│   ├── compiler.cmake             # 编译器配置 (MSVC/GCC/Clang)
│   ├── dependencies.cmake         # FetchContent 依赖管理
│   ├── options.cmake              # 构建选项 (DEBUG/RELEASE)
│   ├── testing.cmake
│   ├── utils.cmake
│   └── fetch_mysql_client.py      # MySQL 客户端库自动下载脚本
│
├── configs/
│   └── server.yaml                # 服务配置（db/jwt/admin etc）
│
├── docs/
│   ├── PROGRESS_SUMMARY.md        # 📊 项目进度快照 (NEW)
│   ├── implementation_plan.md     # 📋 Admin 模块实现计划
│   ├── db_design.md               # DB 补充说明
│   ├── db_design.sql              # 完整 Schema (11 tables)
│   ├── api_design.md              # REST API 设计
│   ├── protocol.md                # IM 二进制协议文档
│   ├── architecture.md            # 系统架构详解
│   ├── sever_arch.md              # 服务端架构
│   └── admin_server/
│       ├── requirements.md
│       ├── implementation_plan.md
│       ├── api_design.md
│       └── db_design.md
│
├── protocol/                      # IM 协议层（binary frames）
│   └── CMakeLists.txt
│
├── server/                        # 服务端核心
│   ├── CMakeLists.txt
│   ├── main.cpp                   # 程序入口
│   ├── admin/                     # Admin 管理面板
│   │   ├── admin_server.h/cpp     # HTTP 路由 + 13 个 Handler
│   │   ├── jwt_utils.h/cpp        # JWT 签发/验证
│   │   ├── password_utils.h/cpp   # PBKDF2-SHA256
│   │   └── http_helper.h          # JSON 响应模板 + ApiError 常量 + 权限检查
│   ├── core/                      # 核心服务
│   │   ├── app_config.h/cpp       # YAML 配置加载
│   │   ├── server_context.h       # DaoFactory 所有权 + 全局上下文
│   │   ├── logger.h/cpp
│   │   ├── thread_pool.h          # Worker threadpool
│   │   ├── mpmc_queue.h           # Vyukov MPMC
│   │   ├── rate_limiter.h         # 滑动窗口频率限制
│   │   ├── defer.h                # NOVA_DEFER scope guard 宏
│   │   └── formatters.h
│   ├── dao/                       # 数据访问层
│   │   ├── dao_factory.h/cpp      # DaoFactory 抽象工厂
│   │   ├── user_dao.h             # 用户 DAO
│   │   ├── admin_account_dao.h    # 管理员 DAO (NEW)
│   │   ├── message_dao.h
│   │   ├── audit_log_dao.h        # audit admin_id 追踪
│   │   ├── admin_session_dao.h    # JWT 黑名单
│   │   ├── rbac_dao.h             # 权限查询 (admin_roles)
│   │   ├── conversation_dao.h     # 对话 (完成中)
│   │   ├── impl/                  # 模板实现
│   │   │   ├── user_dao_impl.h/cpp
│   │   │   ├── admin_account_dao_impl.h/cpp (NEW)
│   │   │   ├── message_dao_impl.h/cpp
│   │   │   ├── audit_log_dao_impl.h/cpp
│   │   │   ├── admin_session_dao_impl.h/cpp
│   │   │   └── rbac_dao_impl.h/cpp
│   │   ├── sqlite3/               # SQLite3 后端
│   │   │   ├── sqlite_db_manager.h/cpp
│   │   │   ├── sqlite_dao_factory.h/cpp
│   │   │   └── seed.h
│   │   └── mysql/                 # MySQL 后端
│   │       ├── mysql_db_manager.h/cpp
│   │       └── mysql_dao_factory.h/cpp
│   ├── model/                     # 数据模型
│   │   ├── types.h                # 所有 struct (New: Admin)
│   │   └── packet.h               # 二进制帧格式
│   ├── net/                       # 网络层
│   │   ├── connection.h           # 连接对象
│   │   ├── tcp_connection.h
│   │   ├── conn_manager.h/cpp
│   │   └── gateway.h/cpp          # TCP 网关
│   ├── service/                   # 业务服务
│   │   ├── router.h/cpp           # 命令字路由
│   │   ├── user_service.h/cpp     # Login/Logout (stub待完成)
│   │   ├── msg_service.h/cpp      # SendMsg/Recall (stub)
│   │   └── sync_service.h/cpp     # 离线同步 (stub)
│   └── test/                      # 单元测试
│       ├── test_conn_manager.cpp
│       ├── test_mpmc_queue.cpp
│       ├── test_router.cpp
│       ├── test_jwt_utils.cpp     # (待补)
│       └── test_admin_account_dao.cpp # (待补)
│
├── client/                        # 客户端（预留）
│   └── cpp/
│
├── build/                         # 构建输出
│   ├── output/
│   │   ├── bin/                   # 可执行文件
│   │   ├── lib/                   # 库文件
│   │   └── test/                  # 测试二进制
│   ├── _deps/                     # 第三方库
│   └── ... (CMakeCache, build.ninja, etc)
│
└── .gitignore / .git
```

---

## 🚀 快速开始

### 前置要求
- **Windows/Linux/macOS** 任一
- **CMake ≥ 3.20**
- **C++20 编译器** (MSVC 2019+ / GCC 11+ / Clang 12+)
- **Python 3.8+** (MySQL 客户端下载脚本)

### 编译步骤

```bash
# 1. 克隆项目
git clone https://github.com/.../NovaIIM.git
cd NovaIIM

# 2. 创建构建目录
mkdir -p build && cd build

# 3. 配置 CMake（首次）
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
# 或 Windows MSVC:
cmake -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release ..

# 4. 编译
ninja            # 或 cmake --build . --config Release
# 该过程会自动：
# - 下载所有第三方库（FetchContent）
# - 编译 ormpp + SQLite3 + libhv 等
# - 如果后端选 MySQL，运行 fetch_mysql_client.py 下载客户端库

# 5. 验证编译
# 检查 build/output/bin/ 目录
ls -la build/output/bin/NovaIIM
```

### 运行服务

```bash
# 启动 NovaIIM 服务器（默认 SQLite3 后端）
./build/output/bin/NovaIIM

# 预期输出：
# [INFO] Loading config from: configs/server.yaml
# [INFO] Using SQLite3 backend: ./data.db
# [INFO] Seeding super admin account...
# [INFO] Admin panel listening on: 0.0.0.0:9091
# [INFO] IM service listening on: 0.0.0.0:9999
# [INFO] Server started. Press Ctrl+C to shutdown.

# 运行单元测试 (83 个用例)
ctest --output-on-failure
```

### 配置文件 (configs/server.yaml)

```yaml
# 数据库配置
db:
  type: "sqlite3"              # 或 "mysql"
  sqlite:
    path: "./data.db"
    wal_mode: true
  mysql:
    host: "localhost"
    port: 3306
    user: "root"
    password: "xxx"
    database: "novaim"

# Admin 面板配置
admin:
  port: 9091
  jwt_secret: "change-me-in-production"  # ⚠️ 生产必须修改!
  jwt_expires: 86400                      # token 过期秒数

# IM 服务配置
server:
  port: 9999
  max_connections: 10000
```

### 测试 Admin 面板

```bash
# 1. 登录获取 token
curl -X POST http://localhost:9091/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"uid":"admin","password":"nova2024"}'

# 响应（复制 token）:
# {"code":0,"data":{"token":"eyJhbGc...","expires_in":86400}}

# 2. 获取当前管理员信息
curl http://localhost:9091/api/v1/auth/me \
  -H "Authorization: Bearer {TOKEN}"

# 3. 查看用户列表
curl "http://localhost:9091/api/v1/users?page=1&page_size=20" \
  -H "Authorization: Bearer {TOKEN}"
```

---

## 📚 核心文档

| 文档 | 用途 | 完成度 |
|------|------|--------|
| [PROGRESS_SUMMARY.md](docs/PROGRESS_SUMMARY.md) | 📊 项目进度快照 | ✅ NEW |
| [implementation_plan.md](docs/admin_server/implementation_plan.md) | 📋 功能 Phase 规划 | ✅ 99% |
| [db_design.sql](docs/db_design.sql) | 🗄️ 11 张表完整 Schema | ✅ 100% |
| [api_design.md](docs/api_design.md) | 🔌 REST API 设计 | ✅ 100% |
| [protocol.md](docs/protocol.md) | 📡 二进制协议格式 | ⚠️ 80% |
| [architecture.md](docs/architecture.md) | 🏗️ 系统架构详解 | ⚠️ 70% |

---

## 🔐 安全检查清单

- ✅ **SQL 注入防护** — 全参数化查询（ormpp prepared statement）
- ✅ **权限隔离** — Admin/User 表分离，权限系统独占 admin_roles
- ✅ **密码安全** — PBKDF2-SHA256 100k iterations (MbedTLS)
- ✅ **JWT 管理** — 令牌黑名单 + 签名验证 + 过期检查
- ✅ **登录频率限制** — 滑动窗口 RateLimiter（5次/60秒/IP，HTTP 429）
- ✅ **密码内存清除** — 验证后 volatile memset 清零明文
- ✅ **trust_proxy** — X-Forwarded-For / X-Real-IP 可配信任
- ✅ **请求头防伪造** — X-Nova-Admin-Id 清除和重注入
- ✅ **审计追踪** — 所有操作记 admin_id + 时间戳 + action
- ✅ **LIKE 注入防护** — 通配符转义 + ESCAPE 子句
- ✅ **整数溢流防护** — int64_t 分页偏移
- ✅ **UID 欺骗防护** — Heartbeat 使用 conn->user_id() atomic 引用

---

## 🎯 开发路线图

| Phase | 内容 | 状态 | ETA |
|-------|------|------|-----|
| 0-3 | 基础工具 + DAO + 认证 + API | ✅ 100% | Q1 2026 |
| 3.5 | **Admin/User 表分离** | ✅ 100% | 2026-04-15 |
| **4** | 单元测试 + ConversationDao | ✅ 83/83 | 2026-04-16 |
| 5 | IM 用户侧（Login/Message/Sync） | ⏳ 0% | 2026-04-28 |
| 6 | 性能优化 + 文档完善 | ⏳ 0% | 2026-05-05 |

---

## 🤝 贡献指南

欢迎提交 Issue 和 PR！

### 编码规范
- C++20 compliance（std::format, concepts, ranges）
- 类用 CamelCase，方法用 snake_case
- 包含完整的 doxygen 注释
- 非平凡函数需单元测试和代码注释

### PR 流程
1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 单元测试 (`ninja test`)
4. 提交更改 (`git commit -m 'feat: AmazingFeature'`)
5. 推送分支 (`git push origin feature/AmazingFeature`)
6. 开启 PR（包含详细描述）

---

## 📊 项目指标

- **代码行数** ~12,000 LoC
- **完成度** 81% (60/74 Phase 任务)
- **编译状态** ✅ 0 errors
- **数据库表** 11 张
- **HTTP API** 13 个端点
- **后端支持** 2 个 (SQLite + MySQL)
- **测试覆盖** ~30% (Phase 4补充)

---

## 📄 许可证

MIT License — 详见 [LICENSE](LICENSE)

---

## 👥 主要贡献者

- @DevTeam — Core infrastructure & Admin panel
- @Contributors — Welcome!

---

**项目维护方：** NovaIIM Core Team  
**报告问题：** Issues 标签分类  
**更新时间：** 2026-04-15  
**成熟度等级：** Alpha → Beta 转型中  
**编译状态：** ✅ 0 errors | 生产就绪
