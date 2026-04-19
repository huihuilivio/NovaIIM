# NovaIIM 系统架构

## 概述

NovaIIM 是一个高性能跨平台 IM 系统，采用 C++20 开发，包含以下子系统：

| 子系统 | 技术栈 | 说明 |
|--------|--------|------|
| IM Server | C++20 单进程 | TCP 网关 + HTTP Admin API |
| Admin 前端 | Vue 3 + TypeScript | 管理面板 Web 应用 |
| IM 客户端 (跨平台) | C++ MVVM + 平台原生 UI | PC / iOS / Android |

### 服务端架构

单进程，由两个独立的网络入口组成：

| 入口 | 协议 | 端口 | 职责 |
|------|------|------|------|
| Gateway | TCP 二进制帧 | 9090 | 客户端 IM 通信 (认证/用户/好友/消息/会话/群组/文件/同步) |
| AdminServer | HTTP JSON | 9091 | 管理面板 API (用户管理/消息管理/审计/仪表盘) |

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言 | C++20, MSVC 2022 / GCC / Clang |
| 网络 | libhv v1.3.3 (TcpServer + HttpServer) |
| ORM | ormpp (header-only, iguana C++20 反射) |
| 数据库 | SQLite3 (WAL 模式, SQLITE_THREADSAFE=1) |
| 日志 | spdlog v1.15.0 |
| 配置 | ylt struct_yaml (C++20 聚合类型反射) |
| JWT | l8w8jwt 2.5.0 + MbedTLS |
| 密码 | PBKDF2-SHA256 (MbedTLS, 100k iterations, iterations cap 10M) |
| 序列化 | ylt struct_pack (C++20 零拷贝二进制序列化) |
| ID 生成 | Snowflake (自定义 worker_id, 41+10+12 bit) |
| 构建 | CMake + Ninja, FetchContent 依赖管理 |

## 数据流

```
客户端 (TCP)                        管理面板 (HTTP)
    │                                    │
    ▼                                    ▼
 Gateway                           AdminServer
    │                                    │
    ▼                                    ▼
 MPMCQueue ──→ ThreadPool          AuthMiddleware (JWT)
                  │                      │
                  ▼                      ▼
               Router               Handlers
                  │                      │
    ┌─────────────┼─────────┐            │
    ▼             ▼         ▼            ▼
 UserSvc      MsgSvc    SyncSvc     DAO Layer (ormpp)
 FriendSvc    ConvSvc   GroupSvc         │
              FileSvc                    │
    │             │         │            │
    └──────┬──────┘         │            │
           ▼                │            ▼
      ConnManager           │       SQLite3 (WAL)
                            │
                       DAO Layer
```

## 线程模型

- **IO 线程** (libhv, 可配): 处理 TCP 连接、拆包
- **MPMCQueue**: Vyukov 无锁有界队列，IO → Worker
- **Worker 线程池** (可配): 执行业务逻辑
- **Admin 线程** (1): HTTP 请求处理

## 模块划分

```
server/
  main.cpp              ← 入口: 配置 → DB → ThreadPool → Gateway → Admin
  core/                 ← 基础设施: 配置/日志/指标/线程池/队列
  net/                  ← 网络层: Connection/ConnManager/Gateway
  model/                ← 数据模型: Packet (二进制帧) / types.h (DB 实体)
  service/              ← 业务逻辑: Router/UserSvc/FriendSvc/MsgSvc/ConvSvc/SyncSvc/GroupSvc/FileSvc
  service/errors/       ← 类型化错误码: common.h (负数系统级) / user_errors.h (正数业务级)
  dao/                  ← 数据访问: DbManager + 各 DAO 实现 (ormpp)
  admin/                ← 管理面板: AdminServer/JWT/密码/响应助手/ApiError常量/Handlers
```

## 安全设计

- JWT 鉴权 + RBAC 权限 + admin_sessions 黑名单
- 全参数化 SQL (ormpp prepared statement)
- PBKDF2-SHA256 密码哈希 + 常量时间比较
- AuthMiddleware 清除伪造的 X-Nova-Admin-Id / X-Nova-Permissions 请求头
- Heartbeat 使用服务端 user_id, 不信任客户端声明
- 写操作审计日志 (action + target + detail + IP)
- 登录频率限制 (RateLimiter: 5次/60秒/IP, HTTP 429)
- 密码内存清除 (volatile memset 清零明文)
- trust_proxy IP 处理 (X-Forwarded-For 可配信任)
- ApiError 类型化错误常量 (32 constexpr, 消除 hardcode)
- TCP 业务错误码: 负数系统级 (kInvalidBody{-1}) + 正数业务级 (命名空间隔离)
- NOVA_DEFER scope guard (事务回滚 / 资源清理)
- in-flight 消息去重超时 (30s timeout 防 TOCTOU)
- 邮箱格式校验 + 密码长度校验 (6-128) + TOCTOU Insert 回退
- PBKDF2 iterations 上限 (10M) 防 CPU DoS
- XFF IP 取最后一跳 (rightmost) 防伪造
- 封禁/删除用户入群校验
- 群成员上限检查 (500)
- 头像路径长度校验 (512)
- Admin middleware 排除 status=deleted 管理员
- 好友申请操作者身份校验
- 批量 DAO 查询消除 N+1 问题

详细架构设计见 [server_arch.md](server_arch.md)，Admin 模块设计见 [admin_server/](admin_server/) 目录。

## Admin 前端架构

```
admin-web/                    ← Vue 3 + TypeScript
├── src/
│   ├── api/                  ← Axios 封装, 各模块 API 调用
│   ├── views/                ← 页面组件 (Login / Dashboard / Users / Messages / AuditLogs)
│   ├── components/           ← 通用组件
│   ├── router/               ← Vue Router + 路由守卫
│   ├── stores/               ← Pinia 状态管理 (auth / user / ...)
│   ├── utils/                ← Token 管理, 格式化工具
│   └── App.vue / main.ts
├── vite.config.ts
└── package.json
```

**技术选型**: Vue 3 + TypeScript + Vite + Element Plus + Pinia + Axios

**对接方式**: HTTP REST → AdminServer (:9091)，JWT Bearer Token 鉴权

## IM 客户端架构 (跨平台 MVVM)

### 分层设计

```
┌────────────────────────────────────────────────────────┐
│                  View (平台原生 UI)                      │
│  ┌──────────┐   ┌──────────────┐   ┌────────────────┐  │
│  │ PC (Qt)  │   │ iOS (Swift   │   │ Android        │  │
│  │ QML      │   │ UIKit/       │   │ Jetpack        │  │
│  │          │   │ SwiftUI)     │   │ Compose)       │  │
│  └────┬─────┘   └──────┬───────┘   └──────┬─────────┘  │
│       │                │                   │            │
│  ═════╪════════════════╪═══════════════════╪══════════  │
│       │        C++ Shared Layer            │            │
│  ┌────┴────────────────┴───────────────────┴─────────┐  │
│  │             ViewModel (C++)                       │  │
│  │  LoginVM · ChatVM · ContactVM · ConvListVM       │  │
│  │  GroupVM · ProfileVM · FileVM · SyncVM           │  │
│  ├───────────────────────────────────────────────────┤  │
│  │             Model (C++)                           │  │
│  │  UserModel · MsgModel · ConvModel · GroupModel   │  │
│  ├───────────────────────────────────────────────────┤  │
│  │             Network (C++)                         │  │
│  │  TcpClient (libhv) · Codec · ReconnectMgr       │  │
│  ├───────────────────────────────────────────────────┤  │
│  │             Local Storage (C++)                   │  │
│  │  SQLite (消息缓存 / 联系人 / 配置)                 │  │
│  └───────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
```

### 核心设计原则

1. **C++ ViewModel + Model**: 业务逻辑、网络通信、本地存储全部用 C++ 实现，编译为共享库
2. **平台 View 各自实现**: PC 用 Qt/QML，iOS 用 SwiftUI，Android 用 Jetpack Compose
3. **Bridge 层**: PC (Qt C++ 直接调用)，iOS (Objective-C++ bridge)，Android (JNI bridge)
4. **协议复用**: 客户端与服务端共用 `protocol/` 目录的 Packet 定义

### 客户端目录结构

```
client/
├── cpp/                      ← C++ 跨平台共享层
│   ├── core/                 ← 日志, 配置, 事件总线
│   ├── net/                  ← TcpClient, Codec, ReconnectMgr, RequestMgr
│   ├── model/                ← 数据模型 + 本地 SQLite 存储
│   ├── viewmodel/            ← ViewModel 层 (登录/聊天/联系人/会话/群/...)
│   └── CMakeLists.txt
├── desktop/                  ← PC 端 (Qt/QML View 层)
├── ios/                      ← iOS 端 (SwiftUI View 层)
└── android/                  ← Android 端 (Compose View 层)
```

### 数据流

```
用户操作 → View → ViewModel::Action()
                     ↓
              Model (业务逻辑)
                     ↓
         ┌───────────┴───────────┐
         ↓                       ↓
   Network (TCP)          LocalStorage (SQLite)
         ↓                       ↓
    Server 响应             本地缓存更新
         ↓                       ↓
         └───────────┬───────────┘
                     ↓
              ViewModel (状态更新)
                     ↓
              View (UI 刷新)
```