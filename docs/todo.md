# NovaIIM Server 待办列表

**最后更新：2026-04-17 | 编译状态：✅ 0 errors | 测试：✅ 222/222**

---

## ✅ 已完成的核心基础设施

### 网络层 ✅
- [x] 实现 Gateway (libhv TCP 网关)
- [x] 多端连接管理 (ConnManager)
- [x] MPMC 消息队列 (Vyukov 算法)
- [x] ThreadPool 消息分发

### 配置和日志 ✅
- [x] 添加配置文件加载 (ylt struct_yaml)
- [x] 集成 spdlog 日志系统
- [x] 自定义 formatter (时间格式等)
- [x] JWT 秘钥校验

### 主程序 ✅
- [x] main.cpp 信号处理/优雅退出
- [x] CreateDaoFactory 根据配置选择后端
- [x] ServerContext 依赖注入中心
- [x] Gateway + ThreadPool 完整集成

---

## ✅ 已完成的数据层

### 数据库引擎 ✅
- [x] ormpp + SQLite3 集成 (WAL + FK)
- [x] ormpp + MySQL 集成 (连接池 + ConnGuard)
- [x] DbManager 封装 (Open/Close/InitSchema)
- [x] MySQL 客户端库自动下载脚本

### Model 层 ✅
- [x] User 完整定义 (id, uid, password_hash, status, ...)
- [x] Admin 完整定义 (新增: 管理员账户) ← NEW
- [x] UserDevice 定义
- [x] Message 定义 (conversation_id + seq)
- [x] Conversation 定义
- [x] AuditLog 定义 (admin_id 操作者)
- [x] AdminSession 定义 (JWT 黑名单)
- [x] Role / Permission / RolePermission / AdminRole 定义

### DAO 层 ✅
- [x] UserDaoImpl (FindByUid / ListUsers / Insert / UpdatePassword / SoftDelete)
- [x] AdminAccountDaoImpl (NEW: 管理员专属) (FindByUid / Insert / UpdatePassword)
- [x] MessageDaoImpl (Insert / GetAfterSeq / ListMessages / UpdateStatus)
- [x] AuditLogDaoImpl (Insert / List 参数化分页)
- [x] AdminSessionDaoImpl (INSERT / IsRevoked / RevokeByAdmin / RevokeByTokenHash)
- [x] RbacDaoImpl (GetUserPermissions [admin_roles 3表JOIN] / HasPermission)
- [x] DaoFactory 抽象工厂 + dual backend (SQLite/MySQL)
- [x] DaoFactory 事务接口 (BeginTransaction/Commit/Rollback)
- [x] UserDao::FindByIds 批量查询

---

## ✅ 已完成的认证和授权

### 认证 + 鉴权层 ✅
- [x] JWT 工具 (l8w8jwt, HS256, Sign/Verify)
- [x] JWT Claims (admin_id 字段)
- [x] PasswordUtils (PBKDF2-SHA256, 100k iterations, MbedTLS)
- [x] AuthMiddleware (JWT 验签 + X-Nova-Admin-Id 注入 + 黑名单查询 + RBAC 权限注入)
- [x] PermissionGuard (RequirePermission 权限检查)

---

## ✅ 已完成的 Admin 管理面板

### 认证端点 ✅
- [x] POST /auth/login (AdminAccountDao 查询 + 密码验证 + JWT 签发 + 会话记录)
- [x] POST /auth/logout (吊销 JWT 令牌 + 审计)
- [x] GET /auth/me (返回管理员信息 + 权限列表)

### 仪表盘 ✅
- [x] GET /dashboard/stats (在线人数 / 消息数 / uptime)

### 用户管理 ✅
- [x] GET /users (分页 + keyword/status 筛选)
- [x] POST /users (创建用户 + 密码哈希 + 审计)
- [x] GET /users/:id (详情 + 在线状态 + 设备列表)
- [x] DELETE /users/:id (软删除 + 踢出 + 审计)
- [x] POST /users/:id/reset-password (修改密码 + 审计)
- [x] POST /users/:id/ban (禁用用户 + 踢出 + 审计)
- [x] POST /users/:id/unban (解禁用户 + 审计)
- [x] POST /users/:id/kick (立即踢出 + 审计)

### 消息管理 ✅
- [x] GET /messages (分页 + 时间/对话 筛选)
- [x] POST /messages/:id/recall (撤回消息 + reason + 审计)

### 审计日志 ✅
- [x] GET /audit-logs (分页 + admin_id/action/时间 筛选 + operator_uid 缓存)

---

## ✅ 已完成的核心特性

### Phase 3.5 — Admin/User 表分离 (2026-04-15) ✅
- [x] 创建 Admin 结构体 (id, uid, password_hash, nickname, status, created_at, updated_at)
- [x] 创建 admins 表 (SQLite + MySQL)
- [x] 实现 AdminAccountDao (FindByUid / FindById / Insert / UpdatePassword)
- [x] 创建 AdminRole 表 (替代 UserRole)
- [x] RBAC 查询改为查 admin_roles (GetUserPermissions)
- [x] JWT Claims: user_id → admin_id
- [x] HTTP Headers: X-Nova-User-Id → X-Nova-Admin-Id
- [x] AuditLog: user_id → admin_id (明确操作者)
- [x] HandleLogin 查询 AdminAccountDao (不再查 User)
- [x] 数据库 Seed 逻辑创建 admins 表初始超管账户
- [x] ServerContext 中心化 DaoFactory 所有权
- [x] 编译状态零错误 (验证通过)
- [x] 所有改动已 commit (c236c0f)

### 双后端一致性 ✅
- [x] SQLite 和 MySQL 的 InitSchema 完全一致
- [x] 所有 DAO impl 使用 template，两个工厂都实例化
- [x] Seed 逻辑双后端通用

---

## ✅ 已完成的测试 (Phase 4)

### 单元测试 — ✅ 已完成 (222 用例)
- [x] JWT 单元测试 (Sign → Verify 往返 / 过期 / 篡改) — 13 用例
- [x] PasswordUtils 测试 (Hash → Verify / 错误密码) — 11 用例
- [x] AdminAccountDao 单元测试 (CRUD 操作) — 7 用例
- [x] AdminSessionDao 单元测试 — 5 用例
- [x] RbacDao 单元测试 (权限查询) — 12 用例
- [x] Handler 集成测试 (HTTP 请求验证) — 21 用例
- [x] Router / MPMC / ConnManager 基础测试 — 15 用例
- [x] UserService 注册/登录测试 — 53 用例
- [x] FriendService 全功能测试 (申请/同意/拒绝/删除/拉黑/列表) — 23 用例
- [x] MsgService 全功能测试 (发送/撤回/送达确认/已读确认) — 22 用例
- [x] ConvService 全功能测试 (列表/删除/免打扰/置顶/多用户隔离) — 23 用例
- [x] Application 启动/数据库初始化测试 — 17 用例

### ConversationDao ✅
- [x] 实现 ConversationDaoImplT 模板 (CreateConversation / IsMember / AddMember / IncrMaxSeq / GetMembersByConversation / GetMembersByUser / UpdateLastAckSeq / UpdateLastReadSeq)
- [x] 添加到 DaoFactory (SqliteDaoFactory / MysqlDaoFactory)
- [x] 集成到 db_manager InitSchema

---

## ✅ IM 用户侧服务 (Phase 5)

### 用户服务 ✅
- [x] UserService::Register (邮箱注册 + 格式校验 + 密码校验 + Snowflake UID + TOCTOU 防护)
- [x] UserService::Login (邮箱登录 + trim/lowercase + 封禁返回通用错误 + 频率限制)
- [x] UserService::Logout (清理会话, 从 ConnManager 移除)
- [x] UserService::Heartbeat (连接心跳, 使用 conn->user_id())
- [x] UserService::SearchUser (邮箱精确 / 昵称模糊搜索, 脱敏)
- [x] UserService::GetProfile / UpdateProfile (个人资料 CRUD, 事务包裹原子更新)

### 好友服务 ✅
- [x] FriendService::AddFriend (发送好友申请, 拉黑/已好友/pending 校验)
- [x] FriendService::HandleRequest (同意/拒绝, 同意时事务包裹: 创建会话 + 双向 friendship)
- [x] FriendService::DeleteFriend (双向标记删除, 保留历史消息)
- [x] FriendService::Block / Unblock (单向拉黑, 解除后设为已删除)
- [x] FriendService::GetFriendList (好友列表 + 批量 FindByIds 优化)
- [x] FriendService::GetRequests (好友申请列表, 分页)
- [x] FriendNotify 推送 (申请/同意/拒绝/删除, 多端推送)

### 消息服务 ✅
- [x] MsgService::SendMsg (消息投递 + seq 递增 + LRU 幂等去重 + in-flight 防 TOCTOU)
- [x] MsgService::RecallMsg (消息撤回 + 可配置时间限制 + 仅发送者 + 广播通知)
- [x] MsgService::DeliverAck (送达确认, 更新 last_ack_seq, 成员校验)
- [x] MsgService::ReadAck (已读确认, 更新 last_read_seq, 成员校验)
- [ ] 多 msg_type 支持 (文本/表情/图片/语音/视频/文件/位置/名片)

### 会话服务 ✅
- [x] ConvService::GetConvList (会话列表 + 未读数 + 最后一条消息摘要 + 私聊对方昵称)
- [x] ConvService::DeleteConv (软隐藏会话, hidden=1, 新消息自动恢复)
- [x] ConvService::MuteConv (免打扰开关, mute 0/1)
- [x] ConvService::PinConv (置顶开关, pinned 0/1)
- [x] BroadcastConvUpdate (ConvUpdate 推送辅助函数)
- [x] MsgService 自动恢复隐藏会话 (发送消息后 unhide)
- [ ] ConvService::DeleteConv (隐藏会话, 新消息自动恢复)
- [ ] ConvService::MuteConv / PinConv (免打扰/置顶)
- [ ] ConvUpdate 推送 (新消息摘要/成员变化/信息变更)

### 群组服务 — 待实现 ⚠️
- [ ] GroupService::CreateGroup (建群 + 自动创建 Conversation)
- [ ] GroupService::DismissGroup (解散群, 仅群主)
- [ ] GroupService::JoinGroup / HandleJoinReq (入群申请 + 审批)
- [ ] GroupService::LeaveGroup (退群, 群主须先转让)
- [ ] GroupService::KickMember (踢出 + 权限校验)
- [ ] GroupService::UpdateGroup (修改群名/头像/公告)
- [ ] GroupService::GetGroupInfo / GetMembers / GetMyGroups
- [ ] GroupService::SetMemberRole (设置管理员)
- [ ] GroupNotify 推送 (创建/解散/加入/退出/踢出/变更/角色)

### 文件服务 — 待实现 ⚠️
- [ ] FileService::Upload (请求上传 → 返回 upload_url + file_id)
- [ ] FileService::UploadComplete (上传完成确认)
- [ ] FileService::Download (请求下载 → 返回 download_url)
- [ ] 秒传 (file_hash 去重)
- [ ] 文件大小限制 (avatar 2MB, image 10MB, file 100MB)

### 同步服务 ✅
- [x] SyncService::SyncMessages (离线消息拉取, 分页 + has_more 标记)
- [x] SyncService::SyncUnread (未读会话 + 未读数 + 最后消息预览, 批量查询优化)

### 输入校验 ✅
- [x] 邮箱格式校验 + 长度限制 (255)
- [x] 密码长度校验 (6-128)
- [x] 昵称长度校验 (100) + 控制字符检测
- [x] 消息内容长度限制 (max_content_size 可配置)
- [ ] 群名/公告长度限制
- [ ] 文件大小限制

---

## 📋 已完成的安全加固

- [x] SQL 注入: 全参数化 (ormpp prepared statement)
- [x] 请求头伪造: AuthMiddleware 清除 X-Nova-*
- [x] UID 欺骗: Heartbeat 用 conn->user_id()
- [x] 数据竞争: user_id_ atomic, device_id_ mutex
- [x] 密码安全: PBKDF2 100k iterations, mbedtls 返回值检查
- [x] 配置安全: JWT 秘钥启动校验
- [x] LIKE 注入: 通配符转义 + ESCAPE
- [x] 整数溢流: Pagination::Offset() → int64_t
- [x] 权限混淆: Admin/User 表分离, admin_roles 独占管理员- [x] 登录频率限制: RateLimiter 滑动窗口 (5次/60秒/IP, HTTP 429)
- [x] 密码内存清除: 验证后 volatile memset 清零明文
- [x] trust_proxy: X-Forwarded-For / X-Real-IP 仅配置启用时信任
- [x] 消息去重超时: in-flight 30s timeout 防 TOCTOU
- [x] ApiError 类型化: 28 个 constexpr 常量，消除 hardcode 字符串
- [x] NOVA_DEFER 宏: Go-style scope guard（事务回滚、资源清理）
- [x] Packet::Encode 校验: body 长度 ≤ kMaxBodySize
- [x] IsRevoked fail-closed: 查询失败视为已吐销
---

## 📚 文档

- [x] 数据库设计 docs/db_design.sql
- [x] Admin 需求文档
- [x] Admin API 设计
- [x] Admin DB 补充设计
- [x] Admin 实现计划 (每周更新)
- [x] 服务端架构文档
- [x] 协议文档
- [x] 系统架构文档
- [ ] API 文档 (Swagger/OpenAPI — 可选)
- [ ] 部署指南 (SQLite vs MySQL 选择)
- [ ] 开发者快速开始指南