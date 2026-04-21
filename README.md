# NovaIIM 🚀

> A High-Performance Next-Gen Instant Messaging System (C++ / CMake)  
> **Current Status:** 265 Tests Passing • 0 Compilation Errors

---

## 📌 项目简介

**NovaIIM（Next Generation IM）** 是一个基于 C++20 构建的高性能即时通讯系统，采用现代 CMake 工程体系。包括：

* ✅ **Admin HTTP REST API** — 13 个端点，完整运维面板
* ✅ **双后端数据库** — SQLite3（开发）+ MySQL 5.7+（生产）
* ✅ **JWT + RBAC 认证** — 精细权限控制，黑名单管理
* ✅ **IM 核心框架** — TCP 网关、多端同步、消息序列化
* ✅ **IM 用户侧服务** — 注册登录/好友/消息/会话/群组/文件/同步，265 测试用例全通过
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
- **11 张表** — users / admins / messages / conversations / conversation_members / audit_logs / admin_sessions / roles / permissions / role_permissions / admin_roles / user_devices / friendships
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

### ✅ 已交付（Phase 5 — IM 完整服务）
- ✅ 单元测试：278 个 C++ 服务端用例 + 14 个客户端用例 + 8 个前端用例全部通过
  - JWT 13 / Password 11 / DAO 24 / API 21 / Router 6 / MPMC 5 / ConnMgr 4 / UserService 53 / FriendService 23 / MsgService 22 / ConvService 23 / GroupService 25 / FileService 20 / SyncService 18 / Application 17
- ✅ **群组服务** (GroupService) — 建群/解散/入群审批/退群/踢人/群信息/成员角色/邀请
- ✅ **文件服务** (FileService) — 上传/下载/共享会话鉴权
- ✅ **同步服务** (SyncService) — 离线消息拉取/未读计数同步

### ⏳ 待补（Phase 6+）
- **运维管理** (Ops Management)：7 个新 API 管理员账户
- **角色管理** (Role Management)：7 个新 API 角色和权限
- 部署指南（SQLite vs MySQL 选择）

### ✅ 已交付（Admin Web 前端 — M1 + M4）
- ✅ **Vue 3 + TypeScript + Element Plus** 管理后台
- ✅ 登录/登出/JWT 认证闭环
- ✅ 6 个管理页面：仪表盘 / 用户管理 / 管理员管理 / 角色权限 / 消息管理 / 审计日志
- ✅ 8 个前端单元测试（Vitest + happy-dom）
- ✅ 开发代理自动转发至后端 :9091

### ✅ 已交付（客户端 C++ 共享库 — M2）
- ✅ **nova_sdk 动态库** (.dll/.so) — MVVM 架构，PIMPL 封装
- ✅ **分层架构**：`infra/` → `core/` → `service/` → `viewmodel/`（仅 viewmodel 导出）
- ✅ Observable\<T\> 数据驱动（线程安全，mutex 保护）
- ✅ NovaClient 单入口 + 缓存 VM 单例（shared_ptr）
- ✅ TcpClient（libhv 封装 + 帧编解码 + 心跳）
- ✅ RequestManager（seq 请求-响应匹配 + 超时）
- ✅ ReconnectManager（指数退避 1s→30s）
- ✅ MsgBus（高性能发布-订阅消息总线）
- ✅ ClientContext（依赖注入 + 服务端推送分发）
- ✅ UIDispatcher（跨线程 UI 调度，平台可注入）
- ✅ DeviceInfo（自动检测设备类型 + FNV-1a 设备 ID）

### ✅ 已交付（WebView2 桌面端 — M3 + 移动端 Bridge — M7）
- ✅ WebView2 桌面端（Win32 窗口 + WebView2 SDK 自动下载）
- ✅ **Vue 3 + TypeScript + Vite + Pinia** 前端（取代原 vanilla JS）
- ✅ 登录/注册页面切换 + 主界面三栏布局
- ✅ Bridge 抽象层（Win32: `__novaBridge` + `chrome.webview.postMessage`，其他平台待定）
- ✅ Pinia Store 封装（auth/chat/connection，Promise-based API）
- ✅ C++ ↔ JS 双向通信桥 (JsBridge)
- ✅ 应用图标（app.ico + app.rc）
- ✅ 线程安全：PostEvent 生命周期守护 + WM_DESTROY 正确关闭顺序
- ✅ iOS Objective-C++ Bridge（NovaClient.h/.mm）
- ✅ Android JNI Bridge（nova_jni.cpp + NovaClient.kt）

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
- **CMake ≥ 3.28** — 唯一构建方式（FetchContent 自动依赖管理）
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
│   ├── db_design.sql              # 完整 Schema (11 tables)
│   ├── todo.md                    # 任务清单
│   ├── protocol.md                # IM 二进制协议文档
│   ├── architecture.md            # 系统架构总览
│   ├── server_arch.md             # 服务端架构详解
│   ├── software_design.md         # 软件设计文档（含 Mermaid 图）
│   ├── admin_server/              # Admin 模块文档
│   │   ├── requirements.md
│   │   ├── implementation_plan.md
│   │   ├── api_design.md
│   │   ├── db_design.md
│   │   └── frontend_design.md
│   ├── im_server/                 # IM 服务端设计文档
│   │   ├── README.md
│   │   ├── design.md
│   │   ├── api.md
│   │   └── db_design.md
│   ├── sdk/                       # 客户端 SDK 文档
│   │   └── README.md              # API 参考 + 架构 + 平台集成
│   └── desktop/                   # 桌面客户端文档
│       └── README.md              # UI 结构 + 通信协议 + 生命周期
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
    │   ├── conversation_dao.h     # 会话 DAO (已完成)
    │   ├── friend_dao.h           # 好友 DAO (已完成)
    │   ├── group_dao.h            # 群组 DAO (已完成)
    │   ├── file_dao.h             # 文件 DAO (已完成)
│   │   ├── impl/                  # 模板实现
│   │   │   ├── user_dao_impl.h/cpp
│   │   │   ├── admin_account_dao_impl.h/cpp (NEW)
│   │   │   ├── message_dao_impl.h/cpp    │   │   ├── conversation_dao_impl.h/cpp
    │   │   ├── friend_dao_impl.h/cpp│   │   │   ├── audit_log_dao_impl.h/cpp
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
    │   ├── service_base.h         # Service 基类
    │   ├── user_service.h/cpp     # 注册/登录/登出/搜索/资料
    │   ├── friend_service.h/cpp   # 好友申请/同意/删除/拉黑/列表
    │   ├── msg_service.h/cpp      # 发送/撤回/送达确认/已读确认
    │   ├── conv_service.h/cpp     # 会话列表/删除/免打扰/置顶
│   │   ├── group_service.h/cpp    # 群组管理 (建群/解散/入群/踢人/角色)
    │   ├── file_service.h/cpp     # 文件上传/下载
│   │   └── sync_service.h/cpp     # 离线同步 (消息拉取/未读计数)
│   └── test/                      # 单元测试
│       ├── test_conn_manager.cpp
│       ├── test_mpmc_queue.cpp
│       ├── test_router.cpp
│       ├── test_jwt_utils.cpp
        ├── test_password_utils.cpp
        ├── test_admin_account_dao.cpp
        ├── test_admin_api.cpp
        ├── test_admin_dao.cpp
        ├── test_application.cpp
        ├── test_user_service.cpp
        ├── test_friend_service.cpp
        ├── test_msg_service.cpp
        ├── test_conv_service.cpp
        ├── test_group_service.cpp
        └── test_file_service.cpp
│   ├── web/                       # Admin 管理后台 (Vue 3 + TS)
│   │   ├── src/
│   │   │   ├── api/               # REST 接口层
│   │   │   ├── layout/            # 主布局（侧边栏 + 顶栏）
│   │   │   ├── router/            # 路由 + 守卫
│   │   │   ├── stores/            # Pinia 状态管理
│   │   │   ├── utils/             # Axios 封装 + Token
│   │   │   └── views/             # 7 个页面视图
│   │   ├── vite.config.ts         # Vite 配置（proxy → :9091）
│   │   └── vitest.config.ts       # 测试配置
│
├── client/                        # 客户端
│   ├── nova_sdk/                  # C++ 共享库 (nova_sdk.dll/.so)
│   │   ├── viewmodel/             # 公共 API (NovaClient, VMs, Observable, types)
│   │   ├── service/               # 业务逻辑 (Auth, Message, Friend, Conv, Group, Sync)
│   │   ├── core/                  # 核心 (ClientContext, Config, RequestManager, MsgBus)
│   │   └── infra/                 # 底层 (TcpClient, HttpClient, Logger, DeviceInfo)
│   ├── desktop/                   # WebView2 桌面客户端 (Windows)
│   │   ├── win/                   # Win32 平台代码
│   │   │   ├── main.cpp           # wWinMain 入口
│   │   │   ├── webview2_app.*     # Win32 窗口 + WebView2 生命周期
│   │   │   ├── win32_ui_dispatcher.*  # PostMessage UI 调度
│   │   │   ├── js_bridge.*        # C++ ↔ JS 双向通信桥
│   │   │   ├── app.rc / app.ico   # 应用图标
│   │   │   └── CMakeLists.txt     # WebView2 SDK 自动下载 + 构建
│   │   └── web/                   # Vue 3 + TypeScript 前端
│   │       ├── package.json       # 依赖 (vue, pinia, vue-router)
│   │       ├── vite.config.ts     # Vite 构建配置
│   │       └── src/               # bridge/ stores/ views/ router/ styles/
│   ├── mobile/                    # 移动端 Bridge
│   │   ├── ios/                   # Objective-C++ (NovaClient.h/.mm)
│   │   └── android/               # JNI (nova_jni.cpp + NovaClient.kt)
│   └── test/                      # SDK 测试（公共 API 级别）
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
- **CMake ≥ 3.28**
- **C++20 编译器** (MSVC 2022+ / GCC 11+ / Clang 12+)
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

# 运行 C++ 单元测试 (278 个用例)
ctest --output-on-failure

# 运行前端单元测试 (8 个用例)
cd server/web && npm run test
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
| [todo.md](docs/todo.md) | 📋 任务清单 | 🔄 |
| [db_design.sql](docs/db_design.sql) | 🗄️ 11 张表完整 Schema | ✅ |
| [admin API](docs/admin_server/api_design.md) | 🔌 REST API 设计 | ✅ |
| [protocol.md](docs/protocol.md) | 📡 IM 二进制协议 | ✅ |
| [architecture.md](docs/architecture.md) | 🏗️ 系统架构 | ✅ |
| [server_arch.md](docs/server_arch.md) | 🖥️ 服务端架构详解 | ✅ |
| [sdk/README.md](docs/sdk/README.md) | 📱 客户端 SDK API + 架构 | ✅ |
| [desktop/README.md](docs/desktop/README.md) | 🖥️ 桌面端架构 + Vue 3 + Bridge | ✅ |

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
