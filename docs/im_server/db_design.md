# NovaIIM IM服务器 - 数据库设计文档

## 1. 数据库总体设计

### 1.1 设计原则

数据库设计遵循规范化（消除冗余、保持一致性）、性能优化（合理使用索引、避免全表扫描）、扩展性（支持分区和分库分表）的原则。所有表都包含充分的时间戳和状态字段以支持可观测性。

### 1.2 库表结构概览

```
NovaIIM (主库)
├── users (用户表)
├── conversations (会话表)
├── messages (消息表) *
├── message_status (消息状态表) *
├── user_devices (用户设备表)
├── admins (管理员表) [来自 Admin Server]
├── audit_logs (审计日志表)
└── ...其他表
```

*: 可按时间分区

---

## 2. 核心表定义

### 2.1 users 表 - 用户

```sql
CREATE TABLE users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    uid VARCHAR(128) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    nickname VARCHAR(128) NOT NULL DEFAULT '',
    avatar VARCHAR(512) DEFAULT '',
    status TINYINT DEFAULT 1,           -- 1正常 2禁用 3已删除
    last_login_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 查询优化索引
CREATE INDEX idx_uid ON users(uid);
CREATE INDEX idx_status ON users(status);
CREATE INDEX idx_created_at ON users(created_at);
```

**字段说明:**

| 字段 | 类型 | 说明 |
|------|------|------|
| id | BIGINT | 主键，用户ID，对外暴露 |
| uid | VARCHAR(128) | 用户唯一标识 (邮箱/手机/用户名) |
| password_hash | VARCHAR(255) | 密码哈希 (bcrypt/PBKDF2) |
| nickname | VARCHAR(128) | 用户昵称，可为空 |
| avatar | VARCHAR(512) | 头像URL |
| status | TINYINT | 状态: 1=正常, 2=禁用, 3=已删除 |
| last_login_at | DATETIME | 最后登录时间 |
| created_at | DATETIME | 创建时间 |
| updated_at | DATETIME | 更新时间 |

**关键索引:**
- `uid`: 用于登录时快速找到用户
- `status`: 用于查询活跃用户

---

### 2.2 conversations 表 - 会话

```sql
CREATE TABLE conversations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    type TINYINT NOT NULL,             -- 1私聊 2群聊
    name VARCHAR(255),
    avatar VARCHAR(255),
    owner_id BIGINT,                   -- 私聊时可为 NULL；群聊以 groups.owner_id 为准
    max_seq BIGINT DEFAULT 0,          -- 并发分配需 UPDATE max_seq=max_seq+1 (行锁)
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
```

### 2.2.1 conversation_members 表 - 会话成员

```sql
CREATE TABLE conversation_members (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    role TINYINT DEFAULT 0,            -- 0成员 1管理员 2群主
    last_read_seq BIGINT DEFAULT 0,
    last_ack_seq BIGINT DEFAULT 0,
    mute TINYINT DEFAULT 0,
    pinned TINYINT DEFAULT 0,          -- 0=不置顶 1=置顶
    hidden TINYINT DEFAULT 0,          -- 0=可见 1=隐藏（新消息自动恢复）
    joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_conv_user (conversation_id, user_id)
);
```

**说明：** 采用 conversation_members 关联表（而非 member_ids 逗号分隔），支持高效的成员查询和 per-member 状态（已读位置、免打扰、置顶等）。

---

### 2.3 messages 表 - 消息

```sql
CREATE TABLE messages (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,
    sender_id BIGINT NOT NULL,
    seq BIGINT NOT NULL,
    msg_type TINYINT NOT NULL,         -- 1文本 2图片 3语音 4视频
    content TEXT,
    encrypted_content BLOB,
    status TINYINT DEFAULT 0,          -- 0正常 1已撤回 2已删除
    client_msg_id VARCHAR(64),         -- 客户端幂等去重
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,

    UNIQUE KEY uk_conv_seq (conversation_id, seq),
    UNIQUE KEY uk_conv_client_msg (conversation_id, client_msg_id),
    KEY idx_conv_time (conversation_id, created_at),
    KEY idx_sender (sender_id)
);
```

**说明：**
- seq 由服务端通过 `conversations.max_seq` 原子递增分配
- status: 0=正常, 1=已撤回, 2=已删除
- client_msg_id 用于客户端幂等去重（同一 conversation 内唯一）

**常用查询：**

```sql
-- 拉取会话最新50条消息
SELECT * FROM messages WHERE conversation_id = 9999 ORDER BY seq DESC LIMIT 50;

-- 增量同步（拉取 seq > last_read_seq 的消息）
SELECT * FROM messages WHERE conversation_id = 9999 AND seq > 100 ORDER BY seq ASC LIMIT 100;
```

---

### 2.4 message_receipts 表 - 已读回执 (可选)

记录每个用户对消息的已读状态。

```sql
CREATE TABLE message_receipts (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    message_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    read_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_msg_user (message_id, user_id)
);
```

在当前实现中，已读状态通过 `conversation_members.last_read_seq` 追踪（按 seq 比较），不依赖此表。此表为后续精细到单条消息的已读回执预留。

---

### 2.5 user_devices 表 - 用户设备

```sql
CREATE TABLE user_devices (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    device_id VARCHAR(128) NOT NULL,
    device_type VARCHAR(32),           -- "iOS", "Android", "Web"
    app_version VARCHAR(16),           -- "1.0.0"
    last_login_at DATETIME,
    last_active_at DATETIME,
    
    INDEX idx_user_id (user_id),
    UNIQUE KEY unique_device (user_id, device_id)
);
```

**字段说明:**

| 字段 | 类型 | 说明 |
|------|------|------|
| id | BIGINT | 主键 |
| user_id | BIGINT | 用户ID (外键) |
| device_id | VARCHAR | 设备唯一标识 |
| device_type | VARCHAR | 设备类型 (iOS/Android/Web/Desktop) |
| app_version | VARCHAR | 应用版本 |
| last_login_at | DATETIME | 最后登录时间 |
| last_active_at | DATETIME | 最后活跃时间 |

**用途:**
- 多设备管理: 用户可同时登录多个设备
- 消息过滤: 避免向同设备内的多个连接重复推送
- 统计分析: 用户设备类型分布

---

### 2.6 audit_logs 表 - 审计日志

```sql
CREATE TABLE audit_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    admin_id BIGINT,                   -- 操作管理员
    action VARCHAR(128) NOT NULL,      -- 操作类型 如 "user.ban"
    target VARCHAR(32),                -- 目标类型 "user", "message"
    target_id BIGINT,                  -- 目标ID
    details LONGTEXT,                  -- 操作详情 (JSON)
    ip_address VARCHAR(45),            -- 操作者IP
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_admin_id (admin_id),
    INDEX idx_created_at (created_at),
    INDEX idx_action (action)
);
```

---

## 3. 外键关系

```
users 表
  ├─ id ← conversations.member_ids (间接，通过字符串解析)
  ├─ id ← messages.sender_id
  ├─ id ← user_devices.user_id
  └─ id ← message_status.receiver_id

conversations 表
  └─ id ← messages.conversation_id

messages 表
  └─ id ← message_status.message_id
```

**建议:** 应用层维护关系，不使用数据库级外键约束 (提高灵活性和性能)

---

## 4. 查询优化

### 4.1 常用查询

**1. 用户登录**
```sql
SELECT id, password_hash FROM users WHERE uid = ? AND status = 1;
-- 使用索引: idx_uid
-- 预期时间: <1ms
```

**2. 用户信息**
```sql
SELECT * FROM users WHERE id = ?;
-- 使用索引: PRIMARY KEY
-- 预期时间: <1ms
```

**3. 会话列表**
```sql
SELECT * FROM conversations 
WHERE member_ids LIKE CONCAT('123', ',%') 
   OR member_ids LIKE CONCAT('%', ',', '123', ',%')
   OR member_ids LIKE CONCAT('%', ',', '123')
ORDER BY last_msg_time DESC LIMIT 20;
-- 预期时间: 10-100ms (取决于会话数)
```

**4. 历史消息 (分页)**
```sql
SELECT * FROM messages 
WHERE conversation_id = ? AND seq <= ?
ORDER BY seq DESC 
LIMIT 50;
-- 使用索引: idx_conversation_id, idx_seq
-- 预期时间: 10-50ms
```

**5. 未读消息计数**
```sql
SELECT conversation_id, COUNT(*) as unread_count
FROM message_status
WHERE receiver_id = ? AND status = 0
GROUP BY conversation_id;
-- 预期时间: 50-200ms
-- 优化: 可使用 Redis 缓存
```

### 4.2 慢查询检测配置

```ini
# my.cnf 或 MySQL 8.0+
[mysqld]
slow_query_log = ON
long_query_time = 0.5          # 超过 500ms 记录
log_queries_not_using_indexes = ON
```

### 4.3 缓存策略

**Redis 缓存:**

```python
# 用户信息缓存
USER_INFO_KEY = f"user:{user_id}"
TTL = 3600  # 1 小时

# 会话列表缓存
CONVERSATION_LIST_KEY = f"convlist:{user_id}"
TTL = 600   # 10 分钟

# 未读计数缓存
UNREAD_COUNT_KEY = f"unread:{user_id}"
TTL = 60    # 1 分钟 (高频变化)
```

---

## 5. 性能基准

### 5.1 查询性能 (MySQL 8.0)

| 查询类型 | 条件 | P95 延迟 | 说明 |
|---------|------|---------|------|
| 登录查询 | uid = ? | <1ms | 使用 UNIQUE 索引 |
| 用户信息 | id = ? | <1ms | 主键查询 |
| 会话列表 | < 100 会话 | 5-10ms | LIKE 查询，可优化 |
| 会话列表 | 100-10k 会话 | 50-200ms | 需考虑分页 |
| 历史消息 | LIMIT 50 | 10-30ms | 使用索引 |
| 全表扫描 | - | 1000+ ms | 避免！ |

### 5.2 存储空间估算

假设 1000 万用户，每用户平均 100 条消息：

| 表 | 记录数 | 平均行大小 | 总大小 | 索引大小 |
|----|--------|---------|--------|----------|
| users | 10M | 300B | 3GB | 200MB |
| conversations | 50M | 500B | 25GB | 1GB |
| messages | 1B | 2KB | 2TB | 100GB |
| message_status | 5B | 50B | 250GB | 20GB |
| **总计** | - | - | **2.3TB** | **121GB** |

**优化:**
- 分库分表: 按 user_id % 10 分成 10 个库
- 消息历史归档: 超过 1 年的消息转移到冷存储
- 消息压缩: 存储压缩后的 content 字段

---

## 6. 备份策略

### 6.1 备份计划

```bash
# 每天 02:00 进行全备份
0 2 * * * mysqldump -u root -p nova_iim > /backup/nova_iim_$(date +%Y%m%d).sql

# 每小时备份二进制日志 (增量备份基础)
0 * * * * cp /var/log/mysql/mysql-bin.* /backup/binlog/
```

### 6.2 恢复流程

```bash
# 1. 恢复全备份
mysql -u root -p nova_iim < /backup/nova_iim_20240315.sql

# 2. 恢复二进制日志 (从备份时间点到故障时间点)
mysqlbinlog /backup/binlog/mysql-bin.000015 \
            /backup/binlog/mysql-bin.000016 | mysql -u root -p nova_iim
```

---

## 7. 数据一致性

### 7.1 消息顺序保证

为确保消息顺序一致，采用 **seq (序列号) 机制**：

```sql
-- 获取下一个 seq (单调递增)
SELECT MAX(seq) + 1 as next_seq FROM messages WHERE conversation_id = ?;

-- 插入消息时使用 seq
INSERT INTO messages (conversation_id, sender_id, seq, content, created_at)
VALUES (?, ?, ?, ?, NOW());
```

**关键点:**
- seq 由服务端生成，不依赖客户端
- seq 在每个 conversation 内单调递增
- 客户端按 seq 排序消息，保证顺序

### 7.2 去重处理

**客户端包重复问题:**

```cpp
// 客户端消息携带 client_seq (客户端序列号)
{
  "conversation_id": 9999,
  "content": "Hello",
  "client_seq": 1001  // 用于去重
}

// 服务端去重表
CREATE TABLE message_dedup (
    conversation_id BIGINT,
    serializer_id BIGINT,
    client_seq INT,
    server_seq BIGINT,
    
    UNIQUE KEY unique_dup (conversation_id, sender_id, client_seq)
);

// 插入前检查去重表
SELECT * FROM message_dedup 
WHERE conversation_id = ? AND sender_id = ? AND client_seq = ?;
```

---

## 8. 数据清理策略

### 8.1 软删除 vs 硬删除

**推荐: 软删除**

```sql
-- 用户删除 (不实际删除，标记状态)
UPDATE users SET status = 3, updated_at = NOW() WHERE id = ?;

-- 消息撤回 (更新状态)
UPDATE messages SET status = 3, updated_at = NOW() WHERE id = ?;
```

**硬删除场景 (谨慎):**
```sql
-- 审计日志过期清理 (> 1 year)
DELETE FROM audit_logs WHERE created_at < DATE_SUB(NOW(), INTERVAL 1 YEAR);

-- 消息历史归档后清理
DELETE FROM messages WHERE created_at < '2023-01-01';
```

---

## 9. 监控和告警

### 9.1 关键指标

```sql
-- 表大小
SELECT TABLE_NAME, ROUND((DATA_LENGTH + INDEX_LENGTH) / 1024 / 1024, 2) as size_mb
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'nova_iim'
ORDER BY size_mb DESC;

-- 慢查询统计
SELECT query, count as execution_count, avg_timer_wait 
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text LIKE '%message%'
ORDER BY sum_timer_wait DESC;

-- 连接数
SHOW STATUS LIKE 'Threads_connected';
SHOW STATUS LIKE 'Max_used_connections';
```

### 9.2 告警规则

| 指标 | 阈值 | 处理 |
|------|------|------|
| 表大小 | > 50GB | 考虑分区/分库 |
| 慢查询数 | > 100/分钟 | 优化查询索引 |
| 数据库连接 | > 80% max | 增加连接池 |
| 磁盘使用 | > 80% | 扩容或清理 |

---

## 附录 A: 完整 Schema

```sql
-- 创建数据库
CREATE DATABASE nova_iim CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE nova_iim;

-- users 表
CREATE TABLE users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    uid VARCHAR(128) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    nickname VARCHAR(128) NOT NULL DEFAULT '',
    avatar VARCHAR(512) DEFAULT '',
    status TINYINT DEFAULT 1,
    last_login_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_uid (uid),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- conversations 表
CREATE TABLE conversations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    type TINYINT NOT NULL,
    member_ids VARCHAR(2048) NOT NULL,
    last_msg_id BIGINT,
    last_msg_time DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY unique_members (member_ids(100), type),
    INDEX idx_last_msg_time (last_msg_time DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- messages 表
CREATE TABLE messages (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,
    sender_id BIGINT NOT NULL,
    seq BIGINT NOT NULL,
    content LONGTEXT NOT NULL,
    msg_type TINYINT DEFAULT 1,
    status TINYINT DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_conversation_id (conversation_id),
    INDEX idx_sender_id (sender_id),
    INDEX idx_seq (seq),
    INDEX idx_created_at (created_at),
    UNIQUE KEY unique_seq_conversation (conversation_id, seq)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- user_devices 表
CREATE TABLE user_devices (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    device_id VARCHAR(128) NOT NULL,
    device_type VARCHAR(32),
    app_version VARCHAR(16),
    last_login_at DATETIME,
    last_active_at DATETIME,
    INDEX idx_user_id (user_id),
    UNIQUE KEY unique_device (user_id, device_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- audit_logs 表
CREATE TABLE audit_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    admin_id BIGINT,
    action VARCHAR(128) NOT NULL,
    target VARCHAR(32),
    target_id BIGINT,
    details LONGTEXT,
    ip_address VARCHAR(45),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_admin_id (admin_id),
    INDEX idx_created_at (created_at),
    INDEX idx_action (action)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

