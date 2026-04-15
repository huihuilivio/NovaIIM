# NovaIIM IM服务器 - TCP协议 API 文档

## 1. 协议基础

### 1.1 帧格式（小端序）

所有TCP通信基于**小端序二进制帧**，格式如下：

```
+-------+-------+-------+------------+------+
| cmd:2 | seq:4 | uid:8 | body_len:4 | body |
+-------+-------+-------+------------+------+
  2 bytes 4 bytes 8 bytes 4 bytes    N bytes
```

**总计: 18 字节固定头 + body_len 字节 body**

| 字段 | 类型 | 字节 | 说明 |
|------|------|------|------|
| cmd | uint16 | 2 | 命令字 (见命令定义) |
| seq | uint32 | 4 | 客户端序列号 (请求-响应配对) |
| uid | uint64 | 8 | 用户 ID (登录后由服务端设置) |
| body_len | uint32 | 4 | body 长度 (0-1048576 字节) |
| body | bytes | N | 业务数据 (JSON/二进制，取决于 cmd) |

### 1.2 拆包机制 (libhv)

采用 **UNPACK_BY_LENGTH_FIELD** 方式自动拆包：

```cpp
length_field_offset = 14      // cmd(2) + seq(4) + uid(8)
length_field_bytes = 4        // body_len 字段大小
length_field_coding = ENCODE_BY_LITTEL_ENDIAN
body_offset = 18              // 固定头 + body_len 字段
package_max_length = 1048576 + 18  // 最大 1 MB + header
```

### 1.3 连接建立

1. **TCP 连接**: 客户端连接到 `127.0.0.1:8888`
2. **发送登录**: 第一个包必须是 `Cmd::kLogin`
3. **验证成功**: 服务端设置 `conn->user_id()` 并响应 `Cmd::kLoginAck`
4. **可进行操作**: 之后可发送其他命令

### 1.4 连接断开

**服务端断开场景:**
- Cmd::kLogout 显式登出
- 心跳超时 (5 次心跳未收到响应)
- 未认证超时 (30秒内未登录成功)
- 协议错误 (非法包格式)
- 数据库错误

**客户端主动断开:**
- 用户退出应用
- 网络切换 (WiFi ↔ 4G)
- 收到 Cmd::0x9999 (服务器停止服务)

---

## 2. 认证相关命令

### 2.1 登录 (Cmd::0x0001 - kLogin)

**方向:** C→S

**body 格式 (JSON):**
```json
{
  "uid": "user123",
  "password": "hashed_pwd",
  "device_id": "iPhone_XXXXXX",
  "app_version": "1.0.0"
}
```

**body 字段说明:**
| 字段 | 类型 | 必须 | 说明 |
|------|------|------|------|
| uid | string | ✓ | 用户ID (邮箱/手机号/用户名) |
| password | string | ✓ | 密码哈希或明文 (取决于客户端实现) |
| device_id | string | ✓ | 设备ID (用于多设备管理) |
| app_version | string | ✗ | 客户端APP版本号 |

**成功响应:**
```
cmd = Cmd::0x0002 (kLoginAck)
seq = 与请求相同
uid = 用户 ID (由服务端设置)
body = JSON:
{
  "code": 0,
  "user_id": 12345,
  "nickname": "Alice",
  "avatar": "https://...",
  "unread_count": 5,
  "msg": "ok"
}
```

**失败响应:**
```
code ≠ 0, 服务端断开连接

错误码:
- 1: uid 为空
- 2: 密码错误
- 3: 用户不存在
- 4: 用户被禁用
- 5: 设备ID无效
- 6: 数据库错误
```

**实现细节:**
```cpp
void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    auto js = nlohmann::json::parse(pkt.body);
    std::string uid = js["uid"];
    std::string password = js["password"];
    
    // 查询用户
    auto user = ctx_.dao().User().FindByUid(uid);
    if (!user) {
        SendError(conn, seq, 3, "user not found");
        conn->Close();
        return;
    }
    
    // 验证密码 (示例: plain text, 实际应使用 bcrypt)
    if (user->password_hash != password) {
        SendError(conn, seq, 2, "wrong password");
        conn->Close();
        return;
    }
    
    // 设置连接状态
    conn->set_user_id(user->id);
    
    // 返回成功
    SendOk(conn, seq, user->id, {
        {"user_id", user->id},
        {"nickname", user->nickname},
        {"avatar", user->avatar},
        {"unread_count", 0}
    });
}
```

---

### 2.2 登出 (Cmd::0x0003 - kLogout)

**方向:** C→S

**body 格式:**
```json
{}  // 空对象即可
```

**响应:**
```
cmd = Cmd::0x0003 (kLogout)
code = 0 (总是成功)
msg = "goodbye"
body = {}
```

**服务端行为:**
1. 清除用户连接记录
2. 更新用户最后在线时间
3. 关闭TCP连接

---

### 2.3 心跳 (Cmd::0x0010 - kHeartbeat)

**方向:** C→S (定时发送，建议每 30 秒)

**body 格式:**
```json
{
  "timestamp": 1650000000000  // 客户端时间戳 (ms)
}
```

**响应:**
```
cmd = Cmd::0x0011 (kHeartbeatAck)
body = {
  "code": 0,
  "server_time": 1650000001234,
  "unread_count": 0
}
```

**超时策略:**
- 服务端记录心跳时间
- 超过 5 分钟无心跳 ⇒ 标记连接为"僵尸"
- 再超过 2 分钟无心跳 ⇒ 主动断开
- 客户端开启网络时需重新登录

---

## 3. 消息相关命令

### 3.1 发送消息 (Cmd::0x0100 - kSendMsg)

**方向:** C→S

**body 格式 (JSON):**
```json
{
  "conversation_id": 9999,
  "content": "Hello, World!",
  "msg_type": 1,
  "extra": {}
}
```

**body 字段说明:**
| 字段 | 类型 | 必须 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✓ | 会话ID (由 server 分配) |
| content | string | ✓ | 消息内容 (≤4KB) |
| msg_type | int | ✗ | 消息类型 (1=文本, 2=图片, 3=语音, 4=视频等) |
| extra | object | ✗ | 扩展字段 (可选的元数据) |

**客户端 seq 规则:**
- 客户端自增 seq (用于排序和去重)
- 服务端返回 SendMsgAck 时包含 server_seq

**成功响应:**
```
cmd = Cmd::0x0101 (kSendMsgAck)
seq = 与请求相同
body = {
  "code": 0,
  "server_seq": 10001,
  "server_time": 1650000001234,
  "msg": "ok"
}
```

**错误响应:**
```
code ≠ 0

错误码:
- 1: 消息内容为空
- 2: 未认证 (未登录)
- 5: 消息过大 (>4KB)
- 6: 会话不存在
- 7: 无权限发送 (被禁言)
```

**消息广播 (接收方在线):**

如果接收方在线，服务端立即作为 Cmd::0x0102 (kPushMsg) 广播：

```
cmd = 0x0102
seq = 0 (推送，无需确认seq)
body = {
  "conversation_id": 9999,
  "sender_id": 123,
  "sender_name": "Alice",
  "content": "Hello, World!",
  "server_seq": 10001,
  "server_time": 1650000001234,
  "msg_type": 1
}
```

**消息存储 (接收方离线):**

服务端保存到数据库，设置 `status=0 (未送达)`:
- 当接收方上线拉取历史消息时获取
- 或服务端可选通过推送服务 (APNS/FCM) 通知

---

### 3.2 已送达确认 (Cmd::0x0103 - kDeliverAck)

**方向:** C→S

**body 格式 (JSON):**
```json
{
  "server_seq": 10001,
  "conversation_id": 9999
}
```

**服务端行为:**
- 更新消息状态: `status=1 (已送达)`
- 通知发送方消息已送达 (可选)

**响应:**
```
cmd = 0x0103
code = 0 (总是成功或忽略)
```

---

### 3.3 已读确认 (Cmd::0x0104 - kReadAck)

**方向:** C→S

**body 格式 (JSON):**
```json
{
  "conversation_id": 9999,
  "read_up_to_seq": 10010  // 已读到的消息序列号
}
```

**服务端行为:**
- 更新所有 seq ≤ read_up_to_seq 的消息: `status=2 (已读)`
- 清除该会话的未读计数

**响应:**
```
cmd = 0x0104
code = 0
```

---

## 4. 同步命令

### 4.1 拉取历史消息 (Cmd::0x0200 - kSyncMsg)

**方向:** C→S

**body 格式 (JSON):**
```json
{
  "conversation_id": 9999,
  "last_seq": 10000,    // 客户端最后收到的 server_seq
  "limit": 50,          // 最多拉取条数 (默认20, 最大100)
  "direction": 0        // 0=向后拉(新消息), 1=向前拉(老消息)
}
```

**响应:**
```
cmd = 0x0201 (kSyncMsgResp)
body = {
  "code": 0,
  "messages": [
    {
      "server_seq": 10001,
      "sender_id": 123,
      "sender_name": "Alice",
      "content": "Hello",
      "msg_type": 1,
      "server_time": 1650000001234,
      "status": 1  // 0=发送中, 1=已送达, 2=已读
    },
    ...
  ],
  "total": 150,  // 该会话总消息数
  "has_more": true
}
```

**场景 1: 首次同步 (新用户上线)**
```json
{
  "conversation_id": 9999,
  "last_seq": 0,  // 0 表示从头开始
  "limit": 50
}
```

**场景 2: 增量同步 (断线重连)**
```json
{
  "conversation_id": 9999,
  "last_seq": 10000,  // 上次收到的最后序列号
  "limit": 50
}
```

**场景 3: 历史消息翻页 (向前拉取)**
```json
{
  "conversation_id": 9999,
  "last_seq": 10000,  // 当前屏幕显示的最早消息
  "direction": 1,     // 向前翻页 (拉取更老消息)
  "limit": 20
}
```

---

### 4.2 拉取未读消息 (Cmd::0x0202 - kSyncUnread)

**方向:** C→S

**body 格式 (JSON):**
```json
{
  "limit": 100  // 最多拉取多少条 (默认100)
}
```

**响应:**
```
cmd = 0x0203 (kSyncUnreadResp)
body = {
  "code": 0,
  "unread_by_conversation": {
    "9999": {
      "count": 15,
      "latest_messages": [
        {
          "server_seq": 10020,
          "sender_id": 456,
          "sender_name": "Bob",
          "content": "你好吗",
          "server_time": 1650000010000,
          "msg_type": 1
        },
        ...  // 最后3-5条消息预览
      ]
    },
    "8888": {
      "count": 3,
      "latest_messages": [...]
    }
  },
  "total_unread": 18  // 全部未读总数
}
```

**用途:**
- 用户上线后快速获取未读统计
- 渲染"消息气泡"展示未读数

---

## 5. 错误处理

### 5.1 通用错误响应

所有命令失败时统一格式：

```
cmd = 与请求相同 (或 0xFFFF)
seq = 与请求相同
body = {
  "code": <非0>,
  "msg": "error message"
}
```

### 5.2 错误码速查表

| 错误码 | 含义 | 处理建议 |
|--------|------|--------|
| 0 | 成功 | - |
| 1 | 参数错误 | 检查请求格式 |
| 2 | 未认证 | 需要先登录 |
| 3 | 用户不存在/密码错误 | 检查凭证 |
| 4 | 用户被禁用 | 联系管理员 |
| 5 | 数据过大/超出限制 | 减少数据量 |
| 6 | 资源不存在 | 资源已被删除 |
| 7 | 权限不足 | 操作被禁止 |
| 8 | 频率限制 | 过于频繁，请稍后 |
| 100 | 数据库错误 | 服务端错误，重试 |
| 101 | 不支持的命令 | 检查 cmd 值 |
| 102 | 协议异常 | 包格式不正确 |

### 5.3 连接状态错误

**示例: 未登录就发送消息**
```
客户端发送: Cmd::0x0100 (kSendMsg)

服务端响应:
{
  "code": 2,
  "msg": "not authenticated, must login first"
}

然后服务端断开连接 (或等待超时后断开)
```

---

## 6. 会话管理

### 6.1 创建会话 (隐式)

发送消息时自动创建会话:

```
conversation 对应关系:
- 1:1 私聊: members = [user_a_id, user_b_id] (sorted)
- N:N 群聊: members = [user_a_id, user_b_id, ..., user_n_id]
```

服务端自动生成 conversation_id：
```cpp
// 1:1 会话
int64_t conv_id = Hash({user_a_id, user_b_id});

// 群聊会话  
int64_t conv_id = Hash({user_1, user_2, ..., user_n});
```

### 6.2 会话列表

*注: 可通过 Admin HTTP API 查询*

```
GET /api/v1/conversations
Response:
{
  "code": 0,
  "data": [
    {
      "conversation_id": 9999,
      "type": 1,  // 1=1:1, 2=群聊
      "members": [123, 456],
      "latest_msg": "你好",
      "latest_time": 1650000010000,
      "unread_count": 3
    },
    ...
  ]
}
```

---

## 7. 客户端实现示例 (伪代码)

### 7.1 连接与登录

```cpp
// 1. 建立 TCP 连接
TcpClient client("127.0.0.1", 8888);
client.OnPacket = [](const Packet& pkt) {
    HandlePacket(pkt);
};

// 2. 发送登录请求
Packet login_pkt;
login_pkt.cmd = 0x0001;
login_pkt.seq = 1;
login_pkt.uid = 0;  // 尚未认证
login_pkt.body = JsonEncode({
    {"uid", "user123"},
    {"password", "pwd_hash"},
    {"device_id", "iPhone_ABC"}
});
client.Send(login_pkt);

// 3. 处理登录响应
void HandlePacket(const Packet& pkt) {
    if (pkt.cmd == 0x0002) {  // kLoginAck
        auto body = JsonDecode(pkt.body);
        if (body["code"] == 0) {
            user_id = body["user_id"];
            // 登录成功，可发送其他命令
            StartHeartbeat();
        }
    }
}
```

### 7.2 发送消息

```cpp
void SendMessage(int64_t conversation_id, const std::string& content) {
    static uint32_t seq = 1;
    
    Packet msg_pkt;
    msg_pkt.cmd = 0x0100;  // kSendMsg
    msg_pkt.seq = seq++;
    msg_pkt.uid = user_id;
    msg_pkt.body = JsonEncode({
        {"conversation_id", conversation_id},
        {"content", content},
        {"msg_type", 1}
    });
    
    client.Send(msg_pkt);
    
    // 等待 SendMsgAck (由回调函数处理)
}

void HandlePacket(const Packet& pkt) {
    if (pkt.cmd == 0x0101) {  // kSendMsgAck
        auto body = JsonDecode(pkt.body);
        if (body["code"] == 0) {
            // 消息发送成功
            int64_t server_seq = body["server_seq"];
            // 更新本地 UI
        }
    }
    else if (pkt.cmd == 0x0102) {  // kPushMsg
        // 收到新消息推送
        auto msg = JsonDecode(pkt.body);
        DisplayMessage(msg);
        
        // 立即回复已送达
        SendDeliverAck(msg["server_seq"]);
    }
}
```

### 7.3 心跳与重连

```cpp
void StartHeartbeat() {
    heartbeat_timer = SetInterval(30000, [=]() {
        Packet hb;
        hb.cmd = 0x0010;  // kHeartbeat
        hb.seq = next_seq++;
        hb.uid = user_id;
        hb.body = JsonEncode({
            {"timestamp", CurrentTimeMs()}
        });
        client.Send(hb);
    });
}

// 断线自动重连
client.OnDisconnect = [=]() {
    ClearHeartbeat();
    ReconnectAfterDelay(1000, exponential_backoff);
};
```

---

## 8. 性能指标

### 8.1 吞吐量

| 场景 | TPS (消息/秒) | 备注 |
|------|--------------|------|
| 本地单机 | 50,000+ | 简单文本消息 |
| 生产环境 | 10,000+ | 含数据库延迟 |
| 高并发 | 可水平扩展 | 多机部署 |

### 8.2 延迟 (P95)

| 操作 | 延迟 | 备注 |
|------|------|------|
| 登录 | 50-100ms | 含密码校验 |
| 发送消息 | 20-50ms | 含数据库插入 |
| 推送消息 | 10-20ms | 本地转发 |
| 拉取消息 | 100-300ms | 数据量依赖 |

### 8.3 资源消耗

| 资源 | 50k 连接 | 100k 连接 |
|------|---------|----------|
| 内存 | ~2GB | ~4GB |
| CPU | 40% | 70% |
| 网络带宽 | ~50Mbps | ~100Mbps |

---

## 9. 故障排查

### 9.1 常见问题

**Q: 连接建立后立即断开**
- A: 可能是心跳超时，检查缺少 Cmd::kHeartbeat

**Q: 消息发送后无响应**
- A: 检查 uid 在帧头是否与登录后的用户ID一致

**Q: 无法拉取历史消息**
- A: conversation_id 可能不存在，或 last_seq 设置错误

**Q: 收到错误码 8 (频率限制)**
- A: 短时间内发送过多消息，需要等待或优化客户端逻辑

### 9.2 调试方法

**启用包日志:**
```yaml
# config.yaml
log:
  level: debug
  pattern: "[%Y-%m-%d %H:%M:%S] [%l] %v"
```

**抓包分析:**
```bash
tcpdump -i lo -w packets.pcap port 8888
# 用 Wireshark 打开 packets.pcap
```

---

## 10. 其他

### 10.1 预留命令字

为将来扩展预留的命令字：

| 范围 | 用途 | 
|------|------|
| 0x0001-0x00FF | 认证/用户 |
| 0x0100-0x01FF | 消息 |
| 0x0200-0x02FF | 同步 |
| 0x0300-0x03FF | 群聊管理 |
| 0x0400-0x04FF | 文件传输 |
| 0x0500-0x09FF | 预留 |
| 0xF000-0xFFFF | 内部使用 |

### 10.2 消息长度限制

| 类型 | 大小 | 说明 |
|------|------|------|
| 单条消息 body | ≤1 MB | TCP 帧 body_len 限制 |
| 消息内容 | ≤4 KB | 应用层限制 |
| 同步响应 | ≤10 MB | 单次拉取最多 100 条消息 |

