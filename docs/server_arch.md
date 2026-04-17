# IM 服务端架构设计（C++版本）

## 1. 设计目标

* 单进程高性能
* 无锁/低锁设计
* 模块解耦
* 可扩展到分布式

---

## 2. 技术选型

| 类别 | 选型 | 说明 |
|------|------|------|
| 语言标准 | C++20 | MSVC 2022 Professional |
| 网络 | libhv v1.3.3 | TcpServer (Gateway) + HttpServer (Admin) |
| ORM | ormpp (header-only) | C++20 iguana 反射, prepared statement 防注入 |
| 数据库 | SQLite3 (amalgamation) | SQLITE_THREADSAFE=1, WAL 模式 |
| 日志 | spdlog v1.15.0 | 控制台 + 文件轮转, 命名 logger |
| 配置 | ylt struct_yaml | C++20 聚合类型自动反射 |
| JWT | l8w8jwt 2.5.0 | HS256/384/512, 自带 MbedTLS |
| 密码 | PBKDF2-SHA256 (MbedTLS) | 100k iterations, 16B salt |
| CLI | CLI11 v2.4.2 | 命令行参数解析 |
| 构建 | CMake + Ninja | FetchContent 管理依赖 |
| 线程模型 | Reactor + 无锁队列 + Worker 线程池 | IO线程 → MPMCQueue → ThreadPool |

---

## 3. 整体架构

```text
     TCP :9090                          HTTP :9091
         │                                  │
  ┌──────▼──────┐                   ┌───────▼───────┐
  │   Gateway   │                   │  AdminServer  │
  │  (libhv TCP)│                   │ (libhv HTTP)  │
  └──────┬──────┘                   └───────┬───────┘
         │                                  │
  ┌──────▼──────┐                   ┌───────▼───────┐
  │ MPMCQueue   │                   │ AuthMiddleware│
  │ (Vyukov)    │                   │  (JWT+RBAC)  │
  └──────┬──────┘                   └───────┬───────┘
         │                                  │
  ┌──────▼──────┐                   ┌───────▼───────┐
  │ ThreadPool  │                   │   Handlers    │
  │ (Worker)    │                   │ (auth/user/   │
  └──────┬──────┘                   │  msg/audit)   │
         │                          └───────┬───────┘
  ┌──────▼──────┐                           │
  │   Router    │                    ┌──────▼──────┐
  └──────┬──────┘                    │  DAO Layer  │
         │                           │  (ormpp)    │
  ┌──────┼──────────┐               └──────┬──────┘
  │      │          │                      │
┌─▼──┐ ┌─▼──┐ ┌────▼───┐          ┌───────▼───────┐
│User│ │Msg │ │Sync    │          │   SQLite3     │
│Svc │ │Svc │ │Svc     │          │   (WAL)       │
└─┬──┘ └─┬──┘ └────┬───┘          └───────────────┘
  │      │          │
  └──────┼──────────┘
         │
  ┌──────▼──────┐
  │ ConnManager │
  └─────────────┘
```

**两条独立数据通路：**
- **左路 (TCP):** 客户端 IM 消息通信 — Gateway → MPMCQueue → ThreadPool → Router → Services
- **右路 (HTTP):** 管理面板 API — AdminServer → JWT 中间件 → Handlers → DAO → SQLite3

共享组件：`ServerContext` (原子指标)、`ConnManager` (连接管理)、`DbManager` (数据库)。

---

## 4. 核心模块设计

---

### 4.1 Gateway

职责：

* TCP 连接管理 (libhv TcpServer)
* 自动拆包 (UNPACK_BY_LENGTH_FIELD, 小端序)
* 心跳检测 (ReadTimeout)
* 连接生命周期 (OnConnection / OnMessage)

```cpp
class Connection {
    std::atomic<int64_t> user_id_;    // 线程安全
    mutable std::mutex   device_mutex_;
    std::string          device_id_;  // mutex 保护
    virtual void Send(const Packet&) = 0;
    virtual void Close() = 0;
};
```

---

### 4.2 ConnManager（多端）

```cpp
// 线程安全: shared_mutex 保护
unordered_map<int64_t, vector<Connection*>>
```

* 同一 user_id 可有多个连接（多设备）
* Add/Remove/GetConns/Kick 接口

---

### 4.3 Router

```cpp
class Router {
    unordered_map<Cmd, Handler> handlers_;
    void Dispatch(ConnectionPtr, Packet&);  // 未注册的 cmd 静默丢弃
};
```

---

### 4.4 AdminServer

职责：

* 独立 HTTP 端口 (默认 9091)
* JWT 鉴权中间件 (l8w8jwt)
* 安全: 入口清除 X-Nova-* 请求头防伪造
* RBAC 权限检查 (精确匹配逗号分隔的 permissions)
* 统一 JSON 响应: `{"code": 0, "msg": "ok", "data": {}}`

**路由结构 (P0):**
```
GET  /healthz                        ← 无需鉴权
POST /api/v1/auth/login              ← 无需鉴权
POST /api/v1/auth/logout             ← 吐销 JWT token
GET  /api/v1/auth/me
GET  /api/v1/dashboard/stats
GET  /api/v1/users
POST /api/v1/users
GET  /api/v1/users/:id
DELETE /api/v1/users/:id
POST /api/v1/users/:id/reset-password
POST /api/v1/users/:id/ban
POST /api/v1/users/:id/unban
POST /api/v1/users/:id/kick
GET  /api/v1/messages
POST /api/v1/messages/:id/recall
GET  /api/v1/audit-logs
```

---

### 4.5 MsgService / ConvService（核心）

MsgService 职责：

* seq 生成 (conversation.max_seq + 1)
* 写 DB
* 推送消息
* ACK 处理
* 发送消息后自动恢复隐藏会话 (unhide)

ConvService 职责：

* 会话列表查询 (GetConvList: 未读数 + 最后消息摘要 + 私聊对方昵称)
* 会话软隐藏 (DeleteConv: hidden=1, 新消息自动恢复)
* 免打扰开关 (MuteConv: mute 0/1)
* 置顶开关 (PinConv: pinned 0/1)
* 会话变更推送 (BroadcastConvUpdate 通知所有成员)

---

### 4.6 SyncService

职责：

* 离线拉取
* 多端同步
* 未读管理

---

### 4.7 DAO 层 (ormpp)

| DAO | 说明 |
|-----|------|
| UserDaoImpl | CRUD, FindByEmail, ListUsers (LIKE 转义), 参数化查询 |
| AdminAccountDaoImpl | FindByUid/FindById/Insert/UpdatePassword (管理员专属) |
| AuditLogDaoImpl | Insert + 多条件参数化分页查询 |
| AdminSessionDaoImpl | JWT 黑名单 (update_some prepared stmt) |
| RbacDaoImpl | 3表 JOIN 获取用户权限 (admin_roles) |
| MessageDaoImpl | Insert/GetAfterSeq/UpdateStatus/ListMessages/FindById |
| ConversationDao | 会话 CRUD + 成员管理 + 未读计算 + mute/pinned/hidden 更新 + FindMember |
| FriendshipDao | 好友申请/同意/拒绝/删除/拉黑 + 关系查询 |
| GroupDao | 建群/解散/入群/退群/踢人 + 群信息 CRUD |
| UserFileDao | 文件元数据 CRUD + hash 秒传查询 |

安全措施：
- 全部使用 `query_s` / `update_some` / `insert` (ormpp 内部 prepared statement)
- LIKE 搜索: 转义 `%` / `_` / `\` + `ESCAPE '\'` 子句
- 无字符串拼接 SQL

---

## 5. 消息流程（核心）

---

### 发送消息

```text
Client A
  ↓
Gateway (IO线程)
  ↓
MPMCQueue (无锁入队)
  ↓
ThreadPool (Worker线程)
  ↓
Router → MsgSvc
  ↓
1. 生成 seq (conversation.max_seq + 1)
2. 写 DB
3. 返回 SEND_ACK → A
4. 推送给 B 所有端 (ConnManager)
5. 推送给 A 其他端
```

---

### ACK处理

```text
DELIVER_ACK → 标记已送达
READ_ACK → 更新 read_cursor
```

---

## 6. seq顺序保证

---

### 方案

```cpp
int64_t seq = repo.GetMaxSeq(conv_id) + 1;  // SQLite 行锁
```

---

### 优化

* 内存缓存 seq
* CAS 自增

---

## 7. 线程模型

```text
IO线程 (libhv, N个)
   ↓
MPMCQueue (Vyukov 无锁有界队列, 容量必须为2的幂)
   ↓
Worker线程池 (ThreadPool, 可配置线程数)
   ↓
业务逻辑 (Router → Services → DAO)
```

**线程安全保障:**
- Connection::user_id_ → `std::atomic<int64_t>` (acquire/release)
- Connection::device_id_ → `std::mutex` 保护
- ConnManager → `shared_mutex` 保护
- ServerContext 所有计数器 → `std::atomic`
- ThreadPool::Stop() → `atomic::exchange` 防重入
- MPMCQueue → Vyukov 无锁算法 + capacity 必须为 2 的幂 (assert)

---

## 8. 无锁队列

```cpp
template <typename T>
class MPMCQueue {
    bool Push(const T& item);  // 拷贝入队
    bool Push(T&& item);       // 移动入队 (PushImpl 模板)
    bool Pop(T& item);         // 移动出队
};
```

用途：

* Gateway IO 线程 → Worker 线程池
* 异步任务提交

---

## 9. 安全设计

| 措施 | 说明 |
|------|------|
| 密码存储 | PBKDF2-SHA256, 100k iterations, 16B random salt, 常量时间比较 |
| JWT 鉴权 | HS256, 可配过期时间, admin_sessions 黑名单 |
| 请求头防伪 | AuthMiddleware 入口清除 X-Nova-Admin-Id / X-Nova-Permissions |
| SQL 注入防护 | ormpp prepared statement, 无字符串拼接 |
| LIKE 注入防护 | 转义 `%`/`_`/`\` + ESCAPE 子句 |
| UID 防伪 | Heartbeat 用服务端 conn->user_id(), 不信任 pkt.uid |
| mbedtls | 所有返回值检查, 失败提前返回 |
| 配置安全 | 启动时校验 JWT 密钥 (默认值警告 + 长度检查) |
| 审计 | 写操作记录到 audit_logs (action + target + detail + IP) |
| 登录频率限制 | RateLimiter 滑动窗口 (5次/60秒/IP, HTTP 429) |
| 密码内存清除 | 验证后 volatile memset 清零明文密码 |
| trust_proxy | X-Forwarded-For / X-Real-IP 仅配置启用时信任 |
| ApiError | 28 个 constexpr 错误常量，消除 hardcode 字符串 |
| NOVA_DEFER | Go-style scope guard 宏 (事务回滚/资源清理) |
| 消息去重 | in-flight 30s timeout 防 TOCTOU, LRU dedup cache |

---

## 10. E2E设计

---

### 服务端

* 不解密
* 不存明文
* 不参与密钥交换

---

### 只负责

```text
转发 ciphertext
```

---

## 10. 多端同步

---

```cpp
for (auto conn : conn_mgr.Get(user_id)) {
    conn->Send(msg);
}
```

---

## 11. 扩展路径

---

### Phase 1（当前）

* 单进程

---

### Phase 2

* Redis（在线状态）
* seq优化

---

### Phase 3

* Gateway拆分
* MsgSvc拆分

---

### Phase 4

* Kafka
* 分库分表

---

## 12. 项目结构

```text
im-server/
├── net/
├── router/
├── service/
├── db/
├── model/
├── proto/
├── util/
```

---

## 13. 总结

该架构具备：

* ✔ 高性能（C++）
* ✔ 可扩展
* ✔ 支持百万连接演进
* ✔ 与Go版本架构一致

---

## 一句话定位

👉 一个可以直接演进成“企业级IM系统”的C++后端架构

---
