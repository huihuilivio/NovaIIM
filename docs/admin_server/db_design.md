# Admin 模块 DB 设计

> **最后更新：** 2026-04-15（Phase 3.5 — Admin/User 表分离）  
> **第一版使用：** SQLite3（零部署依赖），后续可切换到 MySQL 5.7+  
> **完整 Schema：** 详见 [docs/db_design.sql](../db_design.sql)

---

## 核心设计原则

### ✅ 已实现的架构

**Admin/User 完全分离（NEW：2026-04-15）**
- **admins 表**：存储运维人员账户（管理员账户）
- **users 表**：存储 IM 系统的最终用户（业务用户）
- **admin_roles 表**：管理员-角色绑定（独占管理员权限）
- **无 user_roles 表**：用户表不绑定任何权限

**隔离收益：**
1. ✅ 权限混淆风险消除（admins 无法混入 users 权限系统）
2. ✅ 审计追踪清晰（admin_id 明确标记操作者身份）
3. ✅ IM 逻辑简化（users 表完全专注消息业务）
4. ✅ 安全性提升（管理员和用户在数据层物理隔离）

---

## 表结构详解

### 1. admins — 管理员账户表（NEW）

**用途：** 系统管理员和运维人员账户。与 users 表完全独立。

```sql
CREATE TABLE IF NOT EXISTS admins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uid TEXT UNIQUE NOT NULL,                    -- 管理员登录用户名 (admin, operator1, etc)
    password_hash TEXT NOT NULL,                 -- PBKDF2-SHA256 哈希
    nickname TEXT,                               -- 显示名字
    status INTEGER DEFAULT 1,                    -- 1=正常 2=禁用 3=软删除
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_admins_uid ON admins(uid);
CREATE INDEX idx_admins_status ON admins(status);
```

**核心字段说明：**
- `id` - 管理员在系统中的唯一 ID，用于审计追踪（不与 users.id 冲突）
- `uid` - 登录用户名（默认首个管理员为 "admin"）
- `password_hash` - PBKDF2-SHA256 密码（100k iterations）
- `status` - 1=可用 2=禁用 3=软删除（支持后续恢复）

**首次启动初始化：**
- 自动创建 `uid="admin"`, `password="admin123"` (首次运行必改)
- 幂等逻辑：检查 admins 表是否为空，空则插入

---

### 2. admin_roles — 管理员-角色绑定表（NEW，替代 user_roles）

**用途：** 关联 admins 和 roles，管理员的权限来源。

```sql
CREATE TABLE IF NOT EXISTS admin_roles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id INTEGER NOT NULL,                   -- 管理员 ID（来自 admins.id）
    role_id INTEGER NOT NULL,                    -- 角色 ID（来自 roles.id）
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(admin_id, role_id),
    FOREIGN KEY(admin_id) REFERENCES admins(id) ON DELETE CASCADE,
    FOREIGN KEY(role_id) REFERENCES roles(id) ON DELETE CASCADE
);

CREATE INDEX idx_admin_roles_admin ON admin_roles(admin_id);
CREATE INDEX idx_admin_roles_role ON admin_roles(role_id);
```

**关键点：**
- 不存在 `user_roles` 表（users 无权限绑定）
- 权限只通过 admins → admin_roles → roles → permissions 链传递
- 用户没有任何 admin.* 权限（架构级别隔离）

---

### 3. users — IM 最终用户表

**用途：** 只存储 IM 系统的实际用户，无任何权限属性。

```sql
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uid TEXT UNIQUE NOT NULL,                    -- 用户登录 ID
    password_hash TEXT NOT NULL,
    nickname TEXT,
    avatar_url TEXT,
    status INTEGER DEFAULT 1,                    -- 1=正常 2=禁用 3=软删除
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_users_uid ON users(uid);
CREATE INDEX idx_users_status ON users(status);
```

**说明：**
- 不再绑定任何权限
- status=2 意味着被管理员禁用（无法登录 IM）
- status=3 意味着软删除（支持恢复）

---

### 4. messages — 消息表

```sql
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id INTEGER NOT NULL,
    sender_id INTEGER NOT NULL,
    content TEXT NOT NULL,
    status INTEGER DEFAULT 0,                    -- 0=正常 1=已撤回 2=已删除
    seq INTEGER NOT NULL,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(conversation_id) REFERENCES conversations(id),
    FOREIGN KEY(sender_id) REFERENCES users(id)
);

CREATE INDEX idx_messages_conv_seq ON messages(conversation_id, seq);
CREATE INDEX idx_messages_sender ON messages(sender_id);
```

---

### 5. audit_logs — 审计日志表

**用途：** 记录所有管理员操作，通过 `admin_id` 明确追踪操作者。

```sql
CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id INTEGER NOT NULL,                   -- ✨ NEW：操作者（来自 admins.id）
    action TEXT NOT NULL,                        -- user.create, user.ban, msg.recall, etc
    target_type TEXT NOT NULL,                   -- user, message, conversation
    target_id INTEGER,
    detail TEXT,                                 -- JSON格式的额外信息
    ip_address TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(admin_id) REFERENCES admins(id)
);

CREATE INDEX idx_audit_logs_admin ON audit_logs(admin_id);
CREATE INDEX idx_audit_logs_action ON audit_logs(action);
CREATE INDEX idx_audit_logs_created ON audit_logs(created_at);
```

**关键改变：**
- 原 `user_id` 字段改为 `admin_id`（明确指向 admins.id）
- 所有管理员操作都记录在此表
- 用户的消息收发不在此表（IM 系统不审计）
- 可通过 admin_id 追踪哪个管理员进行了什么操作

---

### 6. admin_sessions — JWT 黑名单表

**用途：** 管理 JWT token 的发行和吊销。

```sql
CREATE TABLE IF NOT EXISTS admin_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id INTEGER NOT NULL,                   -- 管理员 ID
    token_hash TEXT NOT NULL UNIQUE,             -- SHA-256(token)
    expires_at TEXT NOT NULL,                    -- ISO-8601 格式
    revoked INTEGER DEFAULT 0,                   -- 0=有效 1=已吊销
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(admin_id) REFERENCES admins(id)
);

CREATE INDEX idx_admin_sessions_token ON admin_sessions(token_hash);
CREATE INDEX idx_admin_sessions_admin ON admin_sessions(admin_id);
CREATE INDEX idx_admin_sessions_expires ON admin_sessions(expires_at);
```

**工作流：**
1. `POST /auth/login` → 生成 JWT token → token_hash 写入此表 (revoked=0)
2. `POST /auth/logout` → 标记 revoked=1
3. 中间件验证 token 时，查询此表确认 revoked=0
4. 过期 token 可定期清理（cron job）

---

### 7. roles — 角色定义表

```sql
CREATE TABLE IF NOT EXISTS roles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    code TEXT UNIQUE NOT NULL,                   -- super_admin, operator, etc
    description TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO roles (name, code, description) VALUES
    ('超级管理员', 'super_admin', '拥有所有权限'),
    ('运营', 'operator', '用户管理 + 消息管理'),
    ('审计', 'auditor', '只读审计日志');
```

---

### 8. permissions — 权限定义表

```sql
CREATE TABLE IF NOT EXISTS permissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    code TEXT UNIQUE NOT NULL,                   -- admin.login, user.view, user.ban, msg.recall, etc
    resource TEXT,                               -- 资源标识（可选）
    action TEXT,                                 -- 动作标识（可选）
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO permissions (name, code) VALUES
    -- Admin 自身权限
    ('管理员登录', 'admin.login'),
    ('查看仪表盘', 'admin.dashboard'),
    ('查看审计日志', 'admin.audit'),
    
    -- 用户管理权限
    ('查看用户', 'user.view'),
    ('创建用户', 'user.create'),
    ('编辑用户', 'user.edit'),
    ('删除用户', 'user.delete'),
    ('禁用用户', 'user.ban'),
    
    -- 消息管理权限
    ('查看消息', 'msg.view'),
    ('撤回消息', 'msg.recall');
```

---

### 9. role_permissions — 角色-权限绑定表

```sql
CREATE TABLE IF NOT EXISTS role_permissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    role_id INTEGER NOT NULL,
    permission_id INTEGER NOT NULL,
    UNIQUE(role_id, permission_id),
    FOREIGN KEY(role_id) REFERENCES roles(id) ON DELETE CASCADE,
    FOREIGN KEY(permission_id) REFERENCES permissions(id) ON DELETE CASCADE
);

-- 超级管理员：所有权限
INSERT INTO role_permissions (role_id, permission_id)
SELECT r.id, p.id FROM roles r, permissions p WHERE r.code = 'super_admin';

-- 运营：部分权限
INSERT INTO role_permissions (role_id, permission_id)
SELECT r.id, p.id FROM roles r, permissions p 
WHERE r.code = 'operator' AND p.code IN (
    'admin.login', 'admin.dashboard',
    'user.view', 'user.ban', 'msg.view', 'msg.recall'
);

-- 审计：只读权限
INSERT INTO role_permissions (role_id, permission_id)
SELECT r.id, p.id FROM roles r, permissions p 
WHERE r.code = 'auditor' AND p.code = 'admin.audit';
```

---

### 其他表（保持不变）

- **conversations** — 对话/群组
- **user_devices** — 用户设备指纹
- **（无 user_roles 表）** — ✨ 删除，用户无权限概念

---

## 数据库初始化流程

```
1. CreateDaoFactory(config)
   └─→ 选择后端 (SQLite / MySQL)
        └─→ DbManager::Open()
             └─→ InitSchema()
                  ├─ CREATE TABLE users
                  ├─ CREATE TABLE admins            ← NEW
                  ├─ CREATE TABLE messages
                  ├─ CREATE TABLE conversations
                  ├─ CREATE TABLE audit_logs        (admin_id 字段)
                  ├─ CREATE TABLE admin_sessions
                  ├─ CREATE TABLE admin_roles       ← NEW (替代 user_roles)
                  ├─ CREATE TABLE roles
                  ├─ CREATE TABLE permissions
                  ├─ CREATE TABLE role_permissions
                  ├─ CREATE TABLE user_devices
                  └─ 建立所有索引
                  
2. SeedSuperAdmin(dao)
   └─→ Check admins count = 0
        └─→ Insert admin (uid="admin", password="admin123")
             └─→ admin_roles bindRole(admin_id=1, role_id=super_admin)
                  └─→ 9 个权限全部绑定完成
```

---

## 权限查询流程（代码侧）

```cpp
// AuthMiddleware 中的权限检查
int64_t admin_id = jwt_claims.admin_id;  // 从 JWT 解析

// 三表 JOIN 查询权限
auto perms = ctx.dao().Rbac().GetUserPermissions(admin_id);
// SELECT DISTINCT p.code FROM permissions p
// JOIN role_permissions rp ON p.id = rp.permission_id
// JOIN roles r ON rp.role_id = r.id
// JOIN admin_roles ar ON r.id = ar.role_id
// WHERE ar.admin_id = ?

// 检查权限
if (!perms.contains("admin.dashboard")) {
    return 403 Forbidden;  // 无权限
}
```

---

## SQLite3 vs MySQL 一致性

### 两个后端的 Schema 完全相同

**SQLite3 特殊处理：**
- `AUTO_INCREMENT` → `AUTOINCREMENT`
- `DATETIME DEFAULT CURRENT_TIMESTAMP` ✓ 支持
- JSON 字段用 TEXT 存储（SQLite JSON1 扩展处理）
- 无 `ON UPDATE CURRENT_TIMESTAMP`（应用层维护）

**MySQL 特殊处理：**
- `AUTO_INCREMENT` ✓ 原生
- `DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP` ✓ 支持
- JSON 原生类型
- 连接池 + ConnGuard RAII

**验证一致性：**
```bash
# SQLite 导出 Schema
sqlite3 data.db ".schema" > sqlite_schema.txt

# MySQL 导出 Schema
mysqldump -u root -p novaim --no-data > mysql_schema.txt

# 对比（去掉类型差异后应该相同）
diff sqlite_schema.txt mysql_schema.txt
```

---

## 安全特性回顾

| 特性 | 实现 | 备注 |
|------|------|------|
| **SQL 注入** | 全参数化查询（ormpp） | ormpp 自动处理 |
| **权限隔离** | admins/admin_roles 独占 | users 无权限属性 |
| **密码安全** | PBKDF2 100k iter | MbedTLS 实现 |
| **操作追踪** | audit_logs.admin_id | 明确操作者身份 |
| **Token 吊销** | admin_sessions.revoked | JWT 黑名单 |
| **软删除** | status字段（1/2/3） | 支持恢复 |
| **外键约束** | FK ON DELETE CASCADE | 数据一致性 |

---

**设计完成日期：** 2026-04-15  
**实现状态：** ✅ 100% 完成 + 双后端验证  
**下一步：** 单元测试（Phase 4）

- `admin_sessions.idx_token_hash`：鉴权中间件每次请求查询黑名单
- 定期清理 `admin_sessions` 中 `expires_at < NOW()` 的过期记录
