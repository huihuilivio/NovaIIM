-- =========================================================
-- IM System + RBAC + Group Permission Design
-- =========================================================

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- =========================================================
-- 1. 用户体系
-- =========================================================

CREATE TABLE users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT, -- 内部关联主键，不对外暴露
    uid VARCHAR(64) NOT NULL UNIQUE,      -- 对外业务 ID（协议/接口层使用）
    password_hash VARCHAR(255) NOT NULL,  -- bcrypt/argon2, 禁止明文
    nickname VARCHAR(100),
    avatar VARCHAR(255),
    status TINYINT DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE user_devices (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    device_id VARCHAR(64) NOT NULL,
    device_type VARCHAR(20),
    last_active_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_user_device (user_id, device_id)
);

-- =========================================================
-- 2. 会话体系
-- =========================================================

CREATE TABLE conversations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    type TINYINT NOT NULL, -- 1私聊 2群聊
    name VARCHAR(255),
    avatar VARCHAR(255),
    owner_id BIGINT,          -- 私聊时可为 NULL；群聊以 groups.owner_id 为准
    max_seq BIGINT DEFAULT 0, -- 并发分配需 UPDATE max_seq=max_seq+1 (行锁)
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE conversation_members (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,

    -- 快速角色标记（Scoped RBAC 未配置时的 fallback）
    role TINYINT DEFAULT 0, -- 0成员 1管理员 2群主; 若启用 conversation_member_roles 则以 RBAC 为准

    last_read_seq BIGINT DEFAULT 0,
    last_ack_seq BIGINT DEFAULT 0,

    mute TINYINT DEFAULT 0,

    joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,

    UNIQUE KEY uk_conv_user (conversation_id, user_id)
);

-- =========================================================
-- 3. 消息体系
-- =========================================================

CREATE TABLE messages (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,

    conversation_id BIGINT NOT NULL,
    sender_id BIGINT NOT NULL,

    seq BIGINT NOT NULL,

    msg_type TINYINT NOT NULL,

    content TEXT,
    encrypted_content BLOB,

    status TINYINT DEFAULT 0, -- 0正常 1已撤回 2已删除

    client_msg_id VARCHAR(64),

    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,

    UNIQUE KEY uk_conv_seq (conversation_id, seq),
    UNIQUE KEY uk_conv_client_msg (conversation_id, client_msg_id), -- 客户端幂等去重
    KEY idx_conv_time (conversation_id, created_at),
    KEY idx_sender (sender_id)
);

-- 可选：已读回执
CREATE TABLE message_receipts (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    message_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    read_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_msg_user (message_id, user_id)
);

-- =========================================================
-- 4. 群扩展
-- =========================================================

CREATE TABLE groups (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL UNIQUE, -- 关联 conversations.id (type=2)
    name VARCHAR(255),
    avatar VARCHAR(255),
    owner_id BIGINT,
    notice TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- =========================================================
-- 5. 好友关系
-- =========================================================

CREATE TABLE friendships (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    friend_id BIGINT NOT NULL,
    status TINYINT DEFAULT 0, -- 0待确认 1已通过 2已拒绝 3已删除 4已拉黑
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_friend (user_id, friend_id)
);

-- =========================================================
-- 6. E2E 加密
-- =========================================================

CREATE TABLE user_identity_keys (
    user_id BIGINT PRIMARY KEY,
    identity_key BLOB NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE user_prekeys (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    prekey_id INT NOT NULL,
    prekey BLOB NOT NULL,
    is_signed TINYINT DEFAULT 0, -- 0=one-time prekey, 1=signed prekey
    used TINYINT DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_user_prekey (user_id, prekey_id)
);

CREATE TABLE session_keys (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    peer_id BIGINT NOT NULL,
    session_key BLOB NOT NULL,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_session (user_id, peer_id)
);

-- =========================================================
-- 7. RBAC（后台权限系统）
-- =========================================================

CREATE TABLE roles (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    code VARCHAR(100) NOT NULL UNIQUE,
    description VARCHAR(255),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE permissions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    code VARCHAR(100) NOT NULL UNIQUE,
    type TINYINT DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE role_permissions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    role_id BIGINT NOT NULL,
    permission_id BIGINT NOT NULL,
    UNIQUE KEY uk_role_perm (role_id, permission_id)
);

CREATE TABLE user_roles (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    role_id BIGINT NOT NULL,
    UNIQUE KEY uk_user_role (user_id, role_id)
);

-- =========================================================
-- 8. 群聊角色权限（Scoped RBAC）
-- =========================================================

CREATE TABLE conversation_roles (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,

    name VARCHAR(100) NOT NULL,
    code VARCHAR(100) NOT NULL,

    is_default TINYINT DEFAULT 0,

    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,

    UNIQUE KEY uk_conv_role (conversation_id, code)
);

CREATE TABLE conversation_role_permissions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    role_id BIGINT NOT NULL,
    permission_id BIGINT NOT NULL,
    UNIQUE KEY uk_role_perm (role_id, permission_id)
);

CREATE TABLE conversation_member_roles (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    conversation_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    role_id BIGINT NOT NULL,
    UNIQUE KEY uk_member_role (conversation_id, user_id, role_id)
);

-- =========================================================
-- 9. 审计日志
-- =========================================================

CREATE TABLE audit_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT,
    action VARCHAR(100) NOT NULL,
    target_type VARCHAR(50), -- user / group / conversation / message
    target_id BIGINT,
    detail JSON, -- 操作详情
    ip VARCHAR(45), -- 支持 IPv6
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    KEY idx_user_action (user_id, action),
    KEY idx_created_at (created_at)
);

-- =========================================================
-- 10. 初始化基础权限（建议）
-- =========================================================

INSERT INTO permissions (name, code) VALUES
('发送消息', 'msg.send'),
('删除自己消息', 'msg.delete_self'),
('删除所有消息', 'msg.delete_all'),
('撤回消息', 'msg.recall'),

('创建群', 'group.create'),
('解散群', 'group.dismiss'),
('邀请成员', 'group.invite'),
('踢人', 'group.kick'),
('禁言', 'group.mute'),
('分配角色', 'group.assign_role'),

('用户查看', 'user.view'),
('用户封禁', 'user.ban'),

('后台登录', 'admin.login'),
('后台首页', 'admin.dashboard');

SET FOREIGN_KEY_CHECKS = 1;