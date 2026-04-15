# NovaIIM 协议设计

## 1. TCP 二进制帧协议

客户端与 Gateway 之间的通信协议，基于 TCP 长连接。

### 帧格式（小端序）

```
+-------+-------+-------+------------+------+
| cmd:2 | seq:4 | uid:8 | body_len:4 | body |
+-------+-------+-------+------------+------+
  总计: 18 字节固定头 + body_len 字节 body
```

| 字段 | 类型 | 字节 | 说明 |
|------|------|------|------|
| cmd | uint16 | 2 | 命令字，见下方定义 |
| seq | uint32 | 4 | 客户端序列号（请求/响应配对） |
| uid | uint64 | 8 | 用户 ID（登录后由服务端认证） |
| body_len | uint32 | 4 | body 长度（最大 1 MB） |
| body | bytes | N | 业务数据（格式取决于 cmd） |

### 拆包机制

libhv `UNPACK_BY_LENGTH_FIELD`:
- `length_field_offset = 14` (cmd + seq + uid)
- `length_field_bytes = 4`
- `length_field_coding = ENCODE_BY_LITTEL_ENDIAN`
- `body_offset = 18` (固定头大小)
- `package_max_length = 18 + 1MB`

### 安全说明

- 服务端不信任帧头中的 `uid` 字段，心跳等操作使用 `conn->user_id()`（登录后服务端设置）
- 未认证连接的 `user_id()` 为 0

---

## 2. 命令字定义

| 命令 | 值 | 方向 | 说明 |
|------|------|------|------|
| **认证** |
| kLogin | 0x0001 | C→S | 登录请求 |
| kLoginAck | 0x0002 | S→C | 登录响应 |
| kLogout | 0x0003 | C→S | 登出请求 |
| **心跳** |
| kHeartbeat | 0x0010 | C→S | 心跳请求 |
| kHeartbeatAck | 0x0011 | S→C | 心跳响应 |
| **消息** |
| kSendMsg | 0x0100 | C→S | 发送消息 |
| kSendMsgAck | 0x0101 | S→C | 发送确认 (含 seq) |
| kPushMsg | 0x0102 | S→C | 推送消息 |
| kDeliverAck | 0x0103 | C→S | 已送达确认 |
| kReadAck | 0x0104 | C→S | 已读确认 |
| **同步** |
| kSyncMsg | 0x0200 | C→S | 拉取历史消息 |
| kSyncMsgResp | 0x0201 | S→C | 历史消息响应 |
| kSyncUnread | 0x0202 | C→S | 拉取未读 |
| kSyncUnreadResp | 0x0203 | S→C | 未读响应 |

---

## 3. Admin HTTP API 协议

独立端口 (默认 9091)，RESTful JSON API。

### 响应格式

```json
{
  "code": 0,
  "msg": "ok",
  "data": { ... }
}
```

### 错误码

| code | 含义 |
|------|------|
| 0 | 成功 |
| 1 | 参数错误 |
| 2 | 未登录 |
| 3 | 无权限 |
| 4 | 资源不存在 |
| 5 | 内部错误 |

### 鉴权

- `Authorization: Bearer <JWT>` (HS256)
- JWT payload: `{sub: "user_id", iss: "nova", iat: ..., exp: ...}`
- 免鉴权路径: `/healthz`, `/api/v1/auth/login`

### 分页

请求: `?page=1&page_size=20` (默认 page=1, page_size=20, 最大 100)

响应:
```json
{
  "items": [...],
  "total": 156,
  "page": 1,
  "page_size": 20
}
```

详细 API 设计见 [admin_server/api_design.md](admin_server/api_design.md)。