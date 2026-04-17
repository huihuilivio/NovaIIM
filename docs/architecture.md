# NovaIIM 系统架构

## 概述

NovaIIM 是一个高性能 IM 服务端，采用 C++20 开发，单进程架构，由两个独立的网络入口组成：

| 入口 | 协议 | 端口 | 职责 |
|------|------|------|------|
| Gateway | TCP 二进制帧 | 9090 | 客户端 IM 通信 (认证/用户/好友/消息/会话/群组/文件/同步) |
| AdminServer | HTTP JSON | 9091 | 管理面板 API (用户管理/消息管理/审计/仪表盘) |

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言 | C++20, MSVC 2022 |
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
 FriendSvc    GroupSvc  FileSvc          │
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
  service/              ← 业务逻辑: Router/UserSvc/FriendSvc/MsgSvc/SyncSvc/GroupSvc/FileSvc
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

详细架构设计见 [server_arch.md](server_arch.md)，Admin 模块设计见 [admin_server/](admin_server/) 目录。