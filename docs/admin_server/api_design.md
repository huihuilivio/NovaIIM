# Admin API 设计

> Base URL: `http://{host}:9091/api/v1`
> Content-Type: `application/json`

## 1. 统一响应格式

```json
{
  "code": 0,
  "msg": "ok",
  "data": { ... }
}
```

错误码：`0`成功 `1`参数错误 `2`未登录 `3`无权限 `4`资源不存在 `5`内部错误

## 2. 统一分页参数

请求：`?page=1&page_size=20`（默认 page=1, page_size=20, 最大100）

响应：
```json
{
  "items": [...],
  "total": 156,
  "page": 1,
  "page_size": 20
}
```

## 3. 认证

### POST /auth/login

管理员登录，返回 JWT。

**Request:**
```json
{
  "uid": "admin",
  "password": "xxx"
}
```

**Response:**
```json
{
  "code": 0,
  "data": {
    "token": "eyJhbGciOi...",
    "expires_in": 86400
  }
}
```

**鉴权方式：** 后续请求携带 `Authorization: Bearer <token>`

### GET /auth/me

获取当前管理员信息 + 权限列表。

**Response:**
```json
{
  "code": 0,
  "data": {
    "user_id": 1,
    "uid": "admin",
    "nickname": "管理员",
    "permissions": ["admin.login", "admin.dashboard", "user.view", "user.ban"]
  }
}
```

---

## 4. 仪表盘

### GET /dashboard/stats

实时运行指标（已有，增强响应格式）。

**权限：** `admin.dashboard`

**Response:**
```json
{
  "code": 0,
  "data": {
    "connections": 128,
    "online_users": 96,
    "messages_in": 12345,
    "messages_out": 23456,
    "bad_packets": 3,
    "uptime_seconds": 86400
  }
}
```

---

## 5. 用户管理

### GET /users

用户列表（分页 + 搜索）。

**权限：** `user.view`

**Query:** `?page=1&page_size=20&keyword=john&status=1`
- `keyword`（可选）：uid 或 nickname 模糊匹配
- `status`（可选）：0全部 1正常 2封禁

**Response:**
```json
{
  "code": 0,
  "data": {
    "items": [
      {
        "id": 1,
        "uid": "john",
        "nickname": "John",
        "avatar": "...",
        "status": 1,
        "is_online": true,
        "created_at": "2026-01-01T00:00:00Z"
      }
    ],
    "total": 156,
    "page": 1,
    "page_size": 20
  }
}
```

### POST /users

添加用户。

**权限：** `user.create`

**Request:**
```json
{
  "uid": "john",
  "password": "initial_password",
  "nickname": "John"
}
```

**Response:**
```json
{
  "code": 0,
  "data": {
    "id": 10,
    "uid": "john"
  }
}
```

### GET /users/:id

用户详情。

**权限：** `user.view`

**Response:**
```json
{
  "code": 0,
  "data": {
    "id": 1,
    "uid": "john",
    "nickname": "John",
    "avatar": "...",
    "status": 1,
    "is_online": true,
    "created_at": "2026-01-01T00:00:00Z",
    "devices": [
      {
        "device_id": "iPhone_xxx",
        "device_type": "iOS",
        "last_active_at": "2026-04-15T10:00:00Z"
      }
    ]
  }
}
```

### DELETE /users/:id

删除用户（软删除，status 置为 3，同时踢下线）。

**权限：** `user.delete`

### POST /users/:id/reset-password

重置用户密码。

**权限：** `user.edit`

**Request:**
```json
{
  "new_password": "reset_password_123"
}
```

### POST /users/:id/ban

封禁用户（同时踢下线）。

**权限：** `user.ban`

**Request:**
```json
{
  "reason": "违规发言"
}
```

### POST /users/:id/unban

解封用户。

**权限：** `user.ban`

### POST /users/:id/kick

踢用户下线（已有，增强审计）。

**权限：** `user.ban`

---

## 6. 消息管理

### GET /messages

消息查询（分页）。

**权限：** `msg.delete_all`

**Query:** `?conversation_id=100&page=1&page_size=50&start_time=2026-04-01&end_time=2026-04-15`

**Response:**
```json
{
  "code": 0,
  "data": {
    "items": [
      {
        "id": 1001,
        "conversation_id": 100,
        "sender_id": 5,
        "sender_uid": "john",
        "seq": 42,
        "msg_type": 1,
        "content": "hello",
        "status": 0,
        "created_at": "2026-04-15T10:00:00Z"
      }
    ],
    "total": 500,
    "page": 1,
    "page_size": 50
  }
}
```

### POST /messages/:id/recall

撤回消息（设置 status=1）。

**权限：** `msg.delete_all`

**Request:**
```json
{
  "reason": "违规内容"
}
```

---

## 7. 审计日志

### GET /audit-logs

查询操作日志（分页）。

**权限：** `admin.dashboard`

**Query:** `?page=1&page_size=20&user_id=1&action=user.ban&start_time=2026-04-01`

**Response:**
```json
{
  "code": 0,
  "data": {
    "items": [
      {
        "id": 1,
        "user_id": 1,
        "operator_uid": "admin",
        "action": "user.ban",
        "target_type": "user",
        "target_id": 5,
        "detail": {"reason": "违规发言"},
        "ip": "192.168.1.1",
        "created_at": "2026-04-15T10:00:00Z"
      }
    ],
    "total": 30,
    "page": 1,
    "page_size": 20
  }
}
```

---

## 8. 健康检查（无需鉴权）

### GET /healthz

```json
{"status": "ok"}
```
