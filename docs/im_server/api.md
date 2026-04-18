# NovaIIM IM服务器 - TCP协议 API 文档

> **权威协议文档已迁移至 [protocol.md](../protocol.md)**
>
> 本文件为客户端集成指南。已实现：认证/用户/好友/消息/会话/群组/文件/同步，265 测试用例全通过。
> 协议细节（命令字、消息体、错误码）以 protocol.md 为准。

---

## 1. 协议概要

- **帧格式：** 18 字节固定头（小端序）+ body（ylt struct_pack 二进制序列化）
- **命令字：** 76 个，覆盖认证/用户/好友/消息/会话/群组/文件/同步
- **Body 序列化：** ylt struct_pack（C++20 零拷贝反射），非 JSON
- **错误码：** 负数为通用系统级错误，正数为各命令专属业务错误

详细帧格式、拆包参数、全部命令字定义见 [protocol.md §1-§2](../protocol.md)。

---

## 2. 连接生命周期

```
1. TCP 连接到 Gateway (默认 :9090)
2. 发送 kLogin (0x0001) — 必须是第一个包
3. 收到 kLoginAck (0x0002, code=0) — 登录成功
4. 启动心跳定时器 (30s 间隔)
5. 正常收发消息
6. 登出: 发送 kLogout (0x0003) 或直接断开
```

**服务端断开场景：**
- `kLogout` 显式登出
- 心跳超时（5 分钟无心跳 → 僵尸标记 → 再 2 分钟断开）
- 未认证超时（30 秒内未登录）
- 协议错误（非法包格式）

---

## 3. 命令分组速查

| 分组 | 命令范围 | 说明 |
|------|---------|------|
| 认证 | 0x0001-0x0006 | 登录/登出/注册 |
| 心跳 | 0x0010-0x0011 | 心跳保活 |
| 用户 | 0x0020-0x0025 | 搜索/资料/修改 |
| 好友 | 0x0030-0x003E | 申请/处理/删除/拉黑/列表/推送 |
| 消息 | 0x0100-0x0107 | 发送/确认/已读/撤回 |
| 会话 | 0x0110-0x011A | 创建/列表/删除/免打扰/置顶/推送 |
| 同步 | 0x0200-0x0203 | 历史消息/未读拉取 |
| 群组 | 0x0500-0x0516 | 建群/入退群/管理/推送 |
| 文件 | 0x0600-0x0605 | 上传/下载 |

---

## 4. 客户端集成要点

### 4.1 登录流程

```
LoginReq {
    email: "user@example.com"  // 不区分大小写，服务端自动小写
    password: "明文密码"        // 6-128 字符
    device_id: "iPhone_ABC"
    device_type: "mobile"
}
→ LoginAck { code=0, user_id, nickname, avatar }
```

- 登录使用邮箱（非 uid），密码传明文（TCP 层应加 TLS）
- 登录成功后 `conn->user_id()` 由服务端设置，后续操作以此为准
- 登录失败（code=2）不区分"密码错误"和"用户不存在"（防枚举）

### 4.2 消息收发

**发送：**
```
SendMsgReq { conversation_id, content, msg_type, client_msg_id }
→ SendMsgAck { code=0, server_seq, server_time }
```

**接收推送：**
```
← PushMsg { conversation_id, sender_id, content, server_seq, server_time, msg_type }
→ DeliverAckReq { conversation_id, server_seq }  // 确认送达
```

**富媒体消息：**
1. 先调用 `kUploadReq` (0x0600) 获取 `file_id` + `upload_url`
2. HTTP PUT 上传文件到 `upload_url`
3. 调用 `kUploadComplete` (0x0602) 确认
4. `SendMsgReq.content` 中引用 `file_id`（JSON 格式，见 protocol.md §3.7.1）

### 4.3 好友流程

```
搜索: SearchUserReq { keyword } → SearchUserAck { users[] }
申请: AddFriendReq { target_user_id, remark } → AddFriendAck
对方收到: ← FriendNotify { type=1 }
同意: HandleFriendReqReq { from_user_id, action=1 } → HandleFriendReqAck { conversation_id }
双方收到: ← FriendNotify { type=2 }
```

### 4.4 群聊流程

```
建群: CreateGroupReq { name, member_ids } → CreateGroupAck { conversation_id, group_id }
成员收到: ← GroupNotify { type=1 }
发消息: SendMsgReq { conversation_id, ... }
成员收到: ← PushMsg { ... }
```

### 4.5 会话管理

```
获取列表: GetConvListReq { page, page_size } → GetConvListAck { conversations[] }
隐藏会话: DeleteConvReq { conversation_id } → DeleteConvAck
免打扰:   MuteConvReq { conversation_id, mute=1 } → MuteConvAck
置顶:     PinConvReq { conversation_id, pinned=1 } → PinConvAck
```

### 4.6 心跳与重连

```
每 30 秒: kHeartbeat (body 可为空) → kHeartbeatAck
断线后:   重连 TCP → 重新 kLogin → kSyncUnread 拉取未读
```

### 4.7 同步流程

```
登录后: kSyncUnread → SyncUnreadResp { items[], total_unread }
  每个有未读的会话:
    kSyncMsg { conversation_id, last_seq=本地最大seq } → SyncMsgResp { messages[], has_more }
    如果 has_more=true: 继续拉取直到 has_more=false
```

**注意事项：**
- `last_seq` 必须 ≥ 0（负数会被拒绝）
- `limit` 默认 20，最大 100
- 返回消息按 seq 升序
- 撤回的消息 status=1，客户端应显示"消息已撤回"

---

## 5. 通用错误码

| code | 含义 | 处理建议 |
|------|------|---------|
| 0 | 成功 | — |
| -1 | 请求体格式无效 | 检查 struct_pack 序列化 |
| -2 | 未认证 | 需要先登录 |
| -100 | 数据库错误 | 服务端错误，可重试 |
| -503 | 服务繁忙 | 过载，退避重试 |

**业务错误码分段（正数）：**

| 范围 | 模块 |
|------|------|
| 1001-1099 | 用户 (登录/注册/搜索/资料) |
| 2001-2099 | 消息 (发送/撤回) |
| 3001-3099 | 文件 (上传/下载) |
| 4001-4099 | 同步 |
| 5001-5099 | 好友 |
| 6001-6099 | 会话管理 |
| 7001-7099 | 群组管理 |

> 各命令的专属业务错误码（正数）见 [protocol.md §3](../protocol.md)。

---

## 6. 性能指标参考

| 指标 | 参考值 |
|------|--------|
| 单机连接数 | 10K+ |
| 消息吞吐 | 5K+ msg/s |
| 登录延迟 | < 50ms (本地 SQLite) |
| 消息投递延迟 | < 10ms (在线用户) |
| 心跳开销 | ~36 bytes/30s/连接 |

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
tcpdump -i lo -w packets.pcap port 9090
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

