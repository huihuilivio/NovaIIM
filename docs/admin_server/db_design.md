# Admin 模块 DB 补充设计

> 第一版使用 **SQLite3**（零部署依赖），后续可切换到 MySQL。
> 主体表结构已在 `docs/db_design.sql` 中定义。本文档仅列出 admin 模块需要的**补充与说明**。
>
> **SQLite3 注意事项：**
> - 使用 WAL 模式提升并发读性能
> - `JSON` 类型改为 `TEXT`（SQLite3 原生 JSON1 扩展已启用）
> - `AUTO_INCREMENT` 改为 `AUTOINCREMENT`
> - `DATETIME DEFAULT CURRENT_TIMESTAMP` 兼容
> - 无 `ON UPDATE CURRENT_TIMESTAMP`，需应用层维护 `updated_at`

## 1. 现有表复用

| 表名 | admin 用途 |
|---|---|
| `users` | admin 账号复用 users 表，通过 RBAC 赋予 `admin.*` 权限区分普通用户与管理员 |
| `roles` / `permissions` / `role_permissions` / `user_roles` | 后台 RBAC 权限控制 |
| `audit_logs` | 所有管理操作写入审计日志 |
| `messages` | 消息查询/撤回，通过 `status` 字段标记撤回 |
| `user_devices` | 用户详情中展示设备信息 |

## 2. 新增表：admin_sessions

用于管理 JWT 黑名单（登出/踢下线时使令牌失效）。

```sql
CREATE TABLE IF NOT EXISTS admin_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    token_hash TEXT NOT NULL,            -- SHA-256(JWT)，用于黑名单比对
    expires_at TEXT NOT NULL,            -- ISO-8601 格式
    revoked INTEGER DEFAULT 0,           -- 0有效 1已吊销
    created_at TEXT DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_admin_sessions_token_hash ON admin_sessions(token_hash);
CREATE INDEX IF NOT EXISTS idx_admin_sessions_user_id ON admin_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_admin_sessions_expires ON admin_sessions(expires_at);
```

## 3. 新增权限种子数据

```sql
-- 补充到 permissions 表的初始化 INSERT 中
INSERT INTO permissions (name, code) VALUES
('审计日志查看', 'admin.audit'),
('用户创建', 'user.create'),
('用户编辑', 'user.edit'),
('用户删除', 'user.delete');
```

## 4. 预置管理员角色

```sql
INSERT INTO roles (name, code, description) VALUES
('超级管理员', 'super_admin', '拥有所有权限'),
('运营', 'operator', '用户管理 + 消息管理');

-- super_admin 绑定所有 admin.* / user.* / msg.delete_all 权限
-- operator 绑定 admin.login, admin.dashboard, user.view, user.ban, msg.delete_all
```

## 5. 审计日志 action 规范

| action | target_type | detail 示例 |
|---|---|---|
| `admin.login` | user | `{"ip": "..."}` |
| `admin.logout` | user | `{}` |
| `user.create` | user | `{"uid": "john"}` |
| `user.delete` | user | `{}` |
| `user.reset_password` | user | `{}` |
| `user.ban` | user | `{"reason": "违规"}` |
| `user.unban` | user | `{}` |
| `user.kick` | user | `{}` |
| `msg.recall` | message | `{"reason": "违规内容", "conversation_id": 100}` |

## 6. 索引说明

- `audit_logs.idx_created_at`：日志按时间范围查询
- `audit_logs.idx_user_action`：按操作者 + 类型筛选
- `admin_sessions.idx_token_hash`：鉴权中间件每次请求查询黑名单
- 定期清理 `admin_sessions` 中 `expires_at < NOW()` 的过期记录
