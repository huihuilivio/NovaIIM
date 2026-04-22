# NovaIIM IM服务器架构设计

> **注意：本文档为设计规划文档。**已实现：Gateway、ConnManager、Router、UserService、FriendService、MsgService、ConvService、GroupService、FileService、SyncService、FileServer（294 测试用例全通过）。

## 1. 系统总体架构

### 1.1 核心组件

```
┌─────────────────────────────────────────────────────────────┐
│                      客户端                                   │
│             (iOS/Android/Web/Desktop)                         │
└──────┬──────────────────────────────────────┬───────────────┘
       │ TCP 长连接                            │ HTTP REST
       ▼                                      ▼
┌─────────────────────────────────────┐  ┌────────────────┐
│              Gateway                 │  │   FileServer   │
│  - TCP Server (libhv)               │  │   (:9092)      │
│  - 连接管理 (ConnManager)            │  │  上传/下载/删除 │
│  - 包解析 (UNPACK_BY_LENGTH_FIELD)  │  │  静态文件预览   │
│  - 分发到 Worker 线程池              │  └───────┬────────┘
└──────────────────────┬──────────────┘          │
                       │                    本地文件系统
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
    ┌────────┐   ┌────────┐   ┌────────┐
    │ Router │   │ Router │   │ Router │
    │Worker1 │   │Worker2 │   │Worker3 │
    └────┬───┘   └────┬───┘   └────┬───┘
         │            │            │
         └──────────┬─┴────────────┘
                    ▼
    ┌───────────────────────────────┐
    │   Service Layer               │
    │ - UserService                 │
    │ - FriendService               │
    │ - MsgService                  │
    │ - ConvService                 │
    │ - GroupService                │
    │ - FileService                 │
    │ - SyncService                 │
    └───────────────┬───────────────┘
                    │
    ┌───────────────┴───────────────┐
    │   DAO Layer (ormpp)           │
    │ - UserDao                     │
    │ - MessageDao                  │
    │ - ConversationDao             │
    │ - ConnectionDao               │
    └───────────────┬───────────────┘
                    │
    ┌───────────────┴───────────────┐
    │   Database Layer              │
    │ - SQLite (开发)               │
    │ - MySQL (生产)                │
    └───────────────────────────────┘
```

### 1.2 运行流程

```
TCP 连接 → Gateway 接收
    ↓
连接建立: ConnManager 记录 (ConnectionPtr)
    ↓
收包: 二进制帧解析 (18字节头 + body)
    ↓
分发: 放入 ThreadPool 工作队列
    ↓
Worker 线程: Router.Dispatch(conn, pkt)
    ↓
Service 层处理 (UserService/FriendService/MsgService/ConvService/GroupService/FileService/SyncService)
    ↓
DAO 层数据操作
    ↓
数据库读写 (SQLite/MySQL)
    ↓
响应封装: 返回 Ack/Push 到客户端
    ↓
连接管理: 心跳、自动断服、重连
```

---

## 2. 连接管理 (Gateway)

### 2.1 功能职责

- **TCP 服务器**: 监听指定端口，接受客户端连接
- **连接生命周期**: 建立 → 活跃 → 断开
- **消息分发**: 根据包的 cmd 字段路由到对应 handler
- **心跳检测**: 定时检查连接活跃状态, 自动断开僵尸连接
- **负载均衡**: 工作队列分发到多个 Worker 线程

### 2.2 架构特性

```cpp
class Gateway {
    std::unique_ptr<hv::HttpServer> server_;  // libhv TCP server
    std::unique_ptr<ConnManager> conn_mgr_;   // 连接管理
    std::function<PacketHandler> handler_;    // 包上报回调
    int worker_threads_;                      // Worker 线程数
};

// 包处理器 (工作线程中执行)
using PacketHandler = std::function<void(ConnectionPtr, Packet&)>;
```

### 2.3 连接状态转移

```
创建 (NEW)
   ↓
已验证 (AUTHENTICATED) ← Cmd::kLogin 成功
   ↓
活跃 (ACTIVE) 
   ↓
断开 (DISCONNECTED) ← Cmd::kLogout 或超时
```

---

## 3. 路由与分发 (Router)

### 3.1 命令路由表

| 命令 | Handler | 服务 | 描述 |
|------|---------|------|------|
| **认证** |
| kLogin (0x0001) | HandleLogin | UserService | 邮箱登录 |
| kLogout (0x0003) | HandleLogout | UserService | 登出 |
| kRegister (0x0005) | HandleRegister | UserService | 邮箱注册 |
| kHeartbeat (0x0010) | HandleHeartbeat | UserService | 心跳 |
| **用户** |
| kSearchUser (0x0020) | HandleSearchUser | UserService | 搜索用户 |
| kGetUserProfile (0x0022) | HandleGetProfile | UserService | 获取资料 |
| kUpdateProfile (0x0024) | HandleUpdateProfile | UserService | 修改资料 |
| **好友** |
| kAddFriend (0x0030) | HandleAddFriend | FriendService | 发送好友申请 |
| kHandleFriendReq (0x0032) | HandleFriendReq | FriendService | 处理申请 |
| kDeleteFriend (0x0034) | HandleDeleteFriend | FriendService | 删除好友 |
| kBlockFriend (0x0036) | HandleBlockFriend | FriendService | 拉黑 |
| kUnblockFriend (0x0038) | HandleUnblockFriend | FriendService | 取消拉黑 |
| kGetFriendList (0x003A) | HandleGetFriendList | FriendService | 好友列表 |
| kGetFriendRequests (0x003C) | HandleGetFriendRequests | FriendService | 申请列表 |
| **消息** |
| kSendMsg (0x0100) | HandleSendMsg | MsgService | 发送消息 |
| kDeliverAck (0x0103) | HandleDeliverAck | MsgService | 已送达确认 |
| kReadAck (0x0104) | HandleReadAck | MsgService | 已读确认 |
| kRecallMsg (0x0105) | HandleRecallMsg | MsgService | 撤回消息 |
| **会话** |
| kCreateConv (0x0110) | HandleCreateConv | ConvService | 创建会话 |
| kGetConvList (0x0112) | HandleGetConvList | ConvService | 会话列表 |
| kDeleteConv (0x0114) | HandleDeleteConv | ConvService | 隐藏会话 |
| kMuteConv (0x0116) | HandleMuteConv | ConvService | 免打扰 |
| kPinConv (0x0118) | HandlePinConv | ConvService | 置顶 |
| **同步** |
| kSyncMsg (0x0200) | HandleSyncMsg | SyncService | 拉取历史消息 |
| kSyncUnread (0x0202) | HandleSyncUnread | SyncService | 拉取未读 |
| **群组** |
| kCreateGroup (0x0400) | HandleCreateGroup | GroupService | 建群 |
| kDismissGroup (0x0402) | HandleDismissGroup | GroupService | 解散群 |
| kJoinGroup (0x0404) | HandleJoinGroup | GroupService | 申请入群 |
| kHandleJoinReq (0x0406) | HandleJoinReq | GroupService | 审批入群 |
| kLeaveGroup (0x0408) | HandleLeaveGroup | GroupService | 退群 |
| kKickMember (0x040A) | HandleKickMember | GroupService | 踢出成员 |
| kGetGroupInfo (0x040C) | HandleGetGroupInfo | GroupService | 群信息 |
| kUpdateGroup (0x040E) | HandleUpdateGroup | GroupService | 修改群信息 |
| kGetGroupMembers (0x0410) | HandleGetGroupMembers | GroupService | 群成员列表 |
| kGetMyGroups (0x0412) | HandleGetMyGroups | GroupService | 我的群列表 |
| kSetMemberRole (0x0414) | HandleSetMemberRole | GroupService | 设置角色 |
| **文件** |
| kUploadReq (0x0500) | HandleUploadReq | FileService | 请求上传 |
| kUploadComplete (0x0502) | HandleUploadComplete | FileService | 上传完成 |
| kDownloadReq (0x0504) | HandleDownloadReq | FileService | 请求下载 |

### 3.2 分发流程

```cpp
// 在 Worker 线程中执行
Router::Dispatch(ConnectionPtr conn, Packet& pkt) {
    1. 查表得到 Handler
    2. 调用 Handler(conn, pkt)
    3. 异常捕获, 返回错误响应
}
```

---

## 4. 应用服务层 (Services)

### 4.1 UserService - 用户管理

**职责:**
- 邮箱注册 (格式/长度/唯一性校验 + Snowflake UID + PBKDF2 密码哈希)
- 邮箱登录 (trim/lowercase + 密码验证 + 封禁检查 + 频率限制)
- 登出处理 (ConnManager 清理)
- 心跳续期
- 用户搜索 (邮箱精确 / 昵称模糊)
- 个人资料管理

### 4.2 FriendService - 好友服务

**职责:**
- 好友申请/同意/拒绝 (双向 friendships 记录)
- 删除好友 (保留历史消息)
- 拉黑/取消拉黑 (单向)
- 好友列表 + 申请列表
- 好友变更推送 (FriendNotify)
- 同意时自动创建私聊 Conversation

### 4.3 MsgService - 消息服务

**职责:**
- 消息发送 (私聊/群聊 + 多 msg_type 支持)
- 消息存储 + 幂等去重 (client_msg_id)
- 送达确认 + 已读确认
- 消息撤回 (时间限制 + 权限校验)
- 消息广播 (online users via ConnManager)
- 离线用户消息入库待同步

**主要操作:**
```cpp
void HandleSendMsg(ConnectionPtr conn, Packet& pkt);      // 发送消息
void HandleDeliverAck(ConnectionPtr conn, Packet& pkt);   // 已送达
void HandleReadAck(ConnectionPtr conn, Packet& pkt);      // 已读
```

**消息流转:**
1. 客户端 → 服务器: Cmd::kSendMsg
2. 服务器 → 客户端: Cmd::kSendMsgAck (确认+seq)
3. 如果接收方在线: Cmd::kPushMsg
4. 接收方 → 服务器: Cmd::kDeliverAck (已送达)
5. 接收方 → 服务器: Cmd::kReadAck (已读)

### 4.4 ConvService - 会话服务

**职责:**
- 会话创建 (私聊自动创建 / 群聊随建群创建)
- 会话列表 (未读数 + 最后消息摘要)
- 隐藏会话 (新消息自动恢复)
- 免打扰 / 置顶
- 会话变更推送 (ConvUpdate)

### 4.5 GroupService - 群组服务

**职责:**
- 建群 / 解散群 (仅群主)
- 入群申请 + 审批
- 退群 / 踢出成员 (权限校验: 群主 > 管理员 > 成员)
- 群信息 CRUD (名称/头像/公告)
- 群成员列表 + 我的群列表
- 群角色管理 (设置/撤销管理员)
- 群事件推送 (GroupNotify)

### 4.6 FileService - 文件服务

**职责:**
- 上传凭证签发 (upload_url + file_id)
- 上传完成确认
- 下载 URL 签发 (有时效性)
- 秒传去重 (file_hash SHA-256)
- 文件大小限制 (avatar 2MB, image 10MB, file 100MB)

> **注意**: FileService 负责 TCP 协议层的文件元数据管理（上传凭证/完成确认/下载URL），实际文件存储由独立的 **FileServer** (HTTP :9092) 提供 REST 接口完成。详见 `server_arch.md §4.5`。

### 4.7 SyncService - 消息同步

**职责:**
- 历史消息拉取 (offline sync)
- 未读消息拉取
- 消息序列号生成 (SEQ)

**主要操作:**
```cpp
void HandleSyncMsg(ConnectionPtr conn, Packet& pkt);      // 拉历史
void HandleSyncUnread(ConnectionPtr conn, Packet& pkt);   // 拉未读
```

**同步场景:**
- 用户上线拉取离线消息
- 用户丢包重新拉取
- 客户端本地数据清空重新同步

---

## 5. 数据访问层 (DAO)

### 5.1 DAO 设计模式

采用 **模板 + 工厂** 模式，支持多后端数据库:

```cpp
// 通用模板
template <typename DbMgr>
class UserDaoImplT : public UserDao {};

// 工厂创建
DaoFactory* factory = CreateDaoFactory(db_config);
UserDao* user_dao = factory->User();
```

### 5.2 主要 DAO 类

| DAO | 职责 | 方法 |
|-----|------|------|
| UserDao | 用户账户 | FindByEmail, FindById, Insert, UpdatePassword, UpdateProfile |
| MessageDao | 消息存储 | Insert, FindByConvId, GetAfterSeq, UpdateStatus, FindById |
| ConversationDao | 会话管理 | Create, FindByUsers, GetConvList, UpdateMember, CalcUnread |
| ConvMemberDao | 会话成员 | Add, Remove, UpdateReadSeq, UpdateMute, UpdatePinned |
| FriendshipDao | 好友关系 | Insert, UpdateStatus, FindByUsers, GetFriendList, GetRequests |
| GroupDao | 群组管理 | Create, Dismiss, UpdateInfo, GetGroupInfo, GetMyGroups |
| UserFileDao | 文件元数据 | Insert, FindById, FindByHash, UpdateStatus |
| DeviceDao | 设备管理 | Insert, FindByUserId, UpdateLastSeen |

### 5.3 DAO 操作示例

```cpp
// 登录时查找用户
std::optional<User> user = ctx.dao().User().FindByUid(uid);
if (!user) {
    // 用户不存在
    SendError(conn, ErrorCode::kUserNotFound);
    return;
}

// 发送消息时保存
Message msg;
msg.sender_id = sender_id;
msg.conversation_id = conv_id;
msg.content = content;
msg.seq = next_seq++;
ctx.dao().Message().Insert(msg);

// 拉取历史消息
auto messages = ctx.dao().Message()
    .FindByConversationId(conv_id, offset, limit);
```

---

## 6. 线程模型

### 6.1 线程池架构

```
Main 线程
    │
    ├─ Gateway (接收 TCP 连接)
    │   └─ ConnManager (连接管理)
    │
    └─ ThreadPool (Worker 线程)
        ├─ Worker #1: 处理包分发
        ├─ Worker #2: 处理包分发
        ├─ Worker #3: 处理包分发
        └─ ...
```

### 6.2 线程安全性

- **Gateway**: 单线程 (libhv event loop)
- **ConnManager**: 线程安全 (自己的同步)
- **ThreadPool**: 线程安全 (工作队列同步)
- **DAO 层**: 线程安全 (连接池)
- **业务逻辑**: 无全局可变状态 (无竞态)

### 6.3 配置参数

```yaml
server:
  port: 9090              # 监听端口
  worker_threads: 8       # Worker 线程数 (通常=CPU核心数)
  queue_capacity: 10000   # 工作队列容量
  idle_timeout: 300       # 连接空闲超时 (秒)
```

---

## 7. 错误处理与恢复

### 7.1 错误码定义

| 错误码 | 场景 | 处理 |
|--------|------|------|
| 1 | 参数错误 (uid=null) | 返回错误 + 断开 |
| 2 | 未认证 (未登录) | 返回错误 + 断开 |
| 3 | 密码错误 | 返回错误 + 断开 |
| 4 | 用户不存在 | 返回错误 + 断开 |
| 5 | 消息过大 (>1MB) | 返回错误 + 继续 |
| 6 | 数据库错误 | 返回错误 + 继续 |

### 7.2 异常处理

```cpp
try {
    router.Dispatch(conn, pkt);
} catch (const std::exception& e) {
    NOVA_LOG_ERROR("Handler exception: {}", e.what());
    SendError(conn, ErrorCode::kInternal);
    conn->Close();  // 断开连接避免状态混乱
}
```

### 7.3 连接恢复

- **心跳检测**: 每 30s 一次心跳，5 次失败则断开
- **自动重连**: 客户端库支持指数退避
- **离线消息**: 服务端保存 7 天内的消息供重新拉取

---

## 8. 性能优化

### 8.1 I/O 优化

- **异步 I/O**: libhv 基于 epoll/kqueue
- **包缓存**: 减少内存分配
- **连接池**: 数据库连接复用

### 8.2 内存优化

- **MPMC 队列**: 高效的工作队列
- **智能指针**: 自动生命周期管理
- **string_view**: 避免不必要的复制

### 8.3 数据库优化

- **索引**: user_id, conversation_id, created_at
- **分区**: 按时间分区消息表 (年/月)
- **批量操作**: 可选 INSERT INTO ... SELECT 批量插入

---

## 9. 扩展性设计

### 9.1 多机部署

```
LB (负载均衡器)
  ├─ IM Server #1 (port 9090)
  ├─ IM Server #2 (port 9090)
  └─ IM Server #3 (port 9090)

共享后端:
  - MySQL 数据库
  - Redis (可选: 会话存储, 未读计数缓存)
  - Kafka (可选: 消息总线, 离线推送)
```

### 9.2 水平扩展

1. 增加 Worker 线程数
2. 增加 IM Server 实例
3. 分库分表 (按 user_id shard)

### 9.3 可观测性

- **日志**: 结构化日志 (spdlog)
- **指标**: Prometheus 集成 (ServerContext)
- **追踪**: 消息 ID 关联链路追踪

---

## 10. 安全性考虑

### 10.1 身份验证

- 用户必须 Cmd::kLogin 才能操作其他命令
- 服务端验证 uid 与密码 (bcrypt/PBKDF2)
- JWT 可选 (用于 Admin 管理面板)

### 10.2 消息隐私

- 消息端到端加密 (可选, 客户端实现)
- 服务端不解密消息内容
- 审计日志记录所有操作

### 10.3 速率限制

- 消息发送: 每用户每秒最多 N 条
- 登录尝试: 每 IP 每 5 分钟最多 5 次
- 连接限制: 每用户同时最多 3 个连接

---

## 11. 监控与告警

### 11.1 关键指标

- **连接数**: 实时在线用户数
- **消息吞吐**: 每秒消息数 (MPS)
- **CPU/内存**: 服务器资源使用率
- **数据库**: 连接数、查询延迟、慢查询

### 11.2 告警阈值

| 指标 | 正常 | 警告 | 严重 |
|------|------|------|------|
| 在线连接 | <50k | 50k-100k | >100k |
| MPS | <10k | 10k-50k | >50k |
| CPU | <50% | 50%-80% | >80% |
| 内存 | <500MB | 500MB-1GB | >1GB |
| DB 延迟 | <10ms | 10-50ms | >50ms |

---

## 12. 开发建议

### 12.1 代码组织

```
server/
  ├── net/          # Gateway 相关
  ├── service/      # 业务逻辑 (UserService, MsgService, SyncService)
  ├── dao/          # 数据访问
  ├── model/        # 数据模型 (User, Message, Packet)
  ├── core/         # 公共组件 (Logger, ThreadPool, Config)
  └── test/         # 单元测试
```

### 12.2 快速开发流程

1. **定义协议**: 在 Packet.h 中新增 Cmd
2. **添加 DAO**: 实现需要的查询操作
3. **实现 Service**: 编写业务逻辑
4. **注册路由**: 在 main.cpp 中注册 handler
5. **测试**: 单元测试 + 集成测试
6. **部署**: 配置 YAML + 启动服务

### 12.3 常见陷阱

- ❌ 在 Gateway 线程中做耗时操作 (应 submit 到工作队列)
- ❌ 不检查用户登录状态就处理操作
- ❌ 消息存储前没有检查消息长度上限
- ❌ 未正确处理数据库连接异常
- ✅ 始终使用 NOVA_LOG_* 记录关键操作
- ✅ 使用智能指针管理内存 (shared_ptr/unique_ptr)
