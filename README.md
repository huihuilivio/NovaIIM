# NovaIIM

> C++20 即时通讯系统，CMake 构建

---

## 项目简介

NovaIIM 是一套完整的即时通讯解决方案，包括：

* **IM 服务端** — TCP 网关、多端同步、消息序列化，支持注册登录/好友/消息/会话/群组/文件/同步
* **Admin 管理面板** — HTTP REST API (13 个端点) + Vue 3 前端，RBAC 权限控制
* **HTTP 文件服务器** — 独立端口 (9092)，小文件/大文件流式上传、下载、删除
* **桌面客户端** — WebView2 + Vue 3，C++ SDK 通过 Bridge 与前端通信
* **移动端 Bridge** — iOS (Objective-C++) 和 Android (JNI) 接口层
* **双后端数据库** — SQLite3（开发环境）+ MySQL 5.7+（生产环境）

---

## 功能概览

### 已完成

**Admin REST API (13 端点):**
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
GET    /audit-logs              操作审计日志
```

**FileServer REST API (端口 9092):**
```
GET    /healthz                        健康检查
GET    /static/**                      静态文件预览/下载
POST   /api/v1/files/upload            小文件上传 (multipart / raw body)
POST   /api/v1/files/upload/{filename} 大文件流式上传
DELETE /api/v1/files/{filename}        删除文件
```

**IM 服务:**
- 用户注册/登录/登出/搜索/资料管理
- 好友申请/同意/拒绝/删除/拉黑/列表
- 消息发送/撤回/送达确认/已读确认/幂等去重
- 会话列表/删除/免打扰/置顶/自动恢复
- 群组创建/解散/入群审批/退群/踢人/角色管理
- 文件上传/下载/共享会话鉴权
- 离线消息同步/未读计数同步

**客户端:**
- C++ SDK 动态库 — MVVM 架构, Observable<T> 数据驱动
- WebView2 桌面端 — Win32 窗口 + Vue 3 前端
- Admin Web 管理后台 — Vue 3 + Element Plus
- iOS/Android Bridge 接口层

### 待完成
- 运维管理 API（管理员账户 CRUD）
- 角色管理 API（角色权限配置）

---

## 系统架构

```
                ┌──────────────────┐  ┌──────────────────┐
                │  Admin HTTP API  │  │   FileServer     │
                │  Port 9091       │  │   Port 9092      │
                └────────┬─────────┘  └────────┬─────────┘
                         │                     │
              ┌──────────▼─────────────────────▼─────┐
              │         NovaIIM Server               │
              │         (C++20 / libhv)              │
              └──────────┬───────────────────────────┘
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

## 技术栈

| 类别 | 技术 | 说明 |
|------|------|------|
| 语言 | C++20 | MSVC 2022 / GCC 11+ / Clang 12+ |
| 构建 | CMake + Ninja | FetchContent 自动依赖管理 |
| 网络 | libhv | TCP + HTTP + WebSocket |
| ORM | ormpp | C++20 reflection，参数化查询 |
| 数据库 | SQLite3 / MySQL 5.7+ | WAL 模式 / 连接池 |
| 认证 | l8w8jwt + MbedTLS | JWT HS256 + PBKDF2-SHA256 |
| 日志 | spdlog | 高性能结构化日志 |
| 序列化 | yalantinglibs | struct_json / struct_yaml / struct_pack |
| 测试 | Google Test | C++ 单元测试 |
| Admin 前端 | Vue 3 + Element Plus | TypeScript + Vite + Pinia |
| 桌面端 | WebView2 + Vue 3 | Win32 窗口 + C++ Bridge |

---

## 项目结构

```
NovaIIM/
├── CMakeLists.txt                 # 顶层构建入口
├── configs/server.yaml            # 服务配置（db / jwt / admin / file_server）
├── protocol/                      # IM 协议层（binary frames）
├── server/                        # 服务端
│   ├── main.cpp
│   ├── admin/                     # Admin 管理面板 (HTTP REST)
│   ├── file/                      # 文件服务器 (HTTP REST, 端口 9092)
│   ├── core/                      # 配置/日志/线程池/安全工具
│   ├── dao/                       # 数据访问层 (ormpp, SQLite/MySQL 双后端)
│   ├── model/                     # 数据模型
│   ├── net/                       # 网络层 (TCP/WS 网关, ConnManager)
│   ├── service/                   # 业务服务 (User/Friend/Msg/Conv/Group/File/Sync)
│   └── test/                      # 单元测试
├── admin-web/                     # Admin 管理后台 (Vue 3 + TypeScript)
├── client/
│   ├── cpp/                       # C++ SDK 共享库 (nova_sdk)
│   ├── desktop/                   # WebView2 桌面客户端
│   ├── mobile/                    # 移动端 Bridge (iOS / Android)
│   └── test/                      # SDK 测试
├── docs/                          # 文档
├── scripts/                       # 构建/部署/测试脚本
└── build/                         # 构建输出
```

---

## 快速开始

### 前置要求
- CMake 3.28+
- C++20 编译器 (MSVC 2022 / GCC 11+ / Clang 12+)
- Python 3.8+（MySQL 客户端下载脚本，选用 MySQL 时需要）

### 编译

```bash
git clone https://github.com/.../NovaIIM.git
cd NovaIIM
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

首次编译会通过 FetchContent 自动拉取所有依赖。

### 运行

```bash
# 启动服务（默认 SQLite3 后端）
./build/bin/im_server --config configs/server.yaml

# 运行单元测试
cd build && ctest --output-on-failure
```

### 配置 (configs/server.yaml)

```yaml
server:
  port: 9090
  worker_threads: 4
  heartbeat_ms: 30000

db:
  type: sqlite                # sqlite / mysql
  path: nova_im.db

admin:
  enabled: true
  port: 9091
  jwt_secret: "change-me-in-production"   # 生产环境务必修改
  jwt_expires: 86400

file_server:
  enabled: true
  port: 9092
  root_dir: files              # 文件存储根目录

log:
  level: debug
  file: ""                    # 留空则仅输出到控制台
```

### 测试 Admin API

```bash
# 登录
curl -X POST http://localhost:9091/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"uid":"admin","password":"nova2024"}'

# 查看用户列表
curl "http://localhost:9091/api/v1/users?page=1&page_size=20" \
  -H "Authorization: Bearer {TOKEN}"
```

---

## 核心文档

| 文档 | 用途 |
|------|------|
| [todo.md](docs/todo.md) | 任务清单与里程碑 |
| [db_design.sql](docs/db_design.sql) | 数据库 Schema (11 张表) |
| [admin API](docs/admin_server/api_design.md) | Admin REST API 设计 |
| [protocol.md](docs/protocol.md) | IM 二进制协议 |
| [architecture.md](docs/architecture.md) | 系统架构总览 |
| [server_arch.md](docs/server_arch.md) | 服务端架构详解 |
| [software_design.md](docs/software_design.md) | 软件设计文档 |
| [sdk/README.md](docs/sdk/README.md) | 客户端 SDK API 参考 |
| [desktop/README.md](docs/desktop/README.md) | 桌面端架构文档 |

---

## 安全设计

- 全参数化查询（ormpp prepared statement），防 SQL 注入
- Admin / User 表物理隔离，权限系统独占 admin_roles
- PBKDF2-SHA256 100k iterations 密码哈希 (MbedTLS)
- JWT 黑名单 + 签名验证 + 过期检查
- 滑动窗口 RateLimiter（5 次 / 60 秒 / IP，返回 HTTP 429）
- 密码验证后 volatile memset 清零明文
- X-Forwarded-For / X-Real-IP 可配信任（trust_proxy）
- X-Nova-Admin-Id 清除并由服务端重注入，防请求头伪造
- 审计日志记录 admin_id + 时间戳 + action
- LIKE 通配符转义 + ESCAPE 子句
- int64_t 分页偏移，防整数溢出
- Heartbeat 使用 conn->user_id() atomic 引用，防 UID 欺骗

---

## 开发路线图

| 阶段 | 内容 | 状态 |
|------|------|------|
| 基础设施 | DAO 抽象工厂 + 认证 + Admin API 13 端点 | 完成 |
| 表分离 | Admin/User 独立表，RBAC 权限隔离 | 完成 |
| 测试 | 294 个单元测试 (GTest + Vitest) | 完成 |
| IM 服务 | 用户/好友/消息/会话/群组/文件/同步 | 完成 |
| 客户端 | SDK + Admin 前端 + 桌面端 + 移动端 Bridge | 完成 |
| 文件服务 | FileServer HTTP REST (端口 9092) | 完成 |
| 运维管理 | 管理员账户 CRUD + 角色权限管理 | 待开始 |

---

## 贡献指南

欢迎提交 Issue 和 PR。

编码规范：
- C++20（std::format, concepts, ranges）
- 类名 CamelCase，方法名 snake_case
- 非平凡函数需有单元测试

PR 流程：
1. Fork 并创建特性分支
2. 编写代码与测试
3. `cmake --build . && ctest` 通过
4. 提交 PR 并附说明

---

## 许可证

MIT License — 详见 [LICENSE](LICENSE)
