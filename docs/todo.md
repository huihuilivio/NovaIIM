# NovaIIM 待办列表

**最后更新：2026-04-21 | 编译状态：✅ 0 errors | 测试：✅ 278/278**

---

## ✅ 已完成的核心基础设施

### 网络层 ✅
- [x] 实现 Gateway (libhv TCP 网关)
- [x] WebSocket 网关 (与 TCP 并行运行, 可配置端口)
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
- [x] MsgBus 发布-订阅总线 (Admin↔IM 解耦, 异步事件分发)
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

## ✅ 已完成的测试 (Phase 4-5)

### 单元测试 — ✅ 已完成 (278 用例)
- [x] JWT 单元测试 (Sign → Verify 往返 / 过期 / 篡改) — 13 用例
- [x] PasswordUtils 测试 (Hash → Verify / 错误密码) — 11 用例
- [x] AdminAccountDao 单元测试 (CRUD 操作) — 9 用例
- [x] AdminSessionDao 单元测试 — 6 用例
- [x] RbacDao 单元测试 (权限查询) — 9 用例
- [x] Handler 集成测试 (HTTP 请求验证) — 21 用例
- [x] Router / MPMC / ConnManager 基础测试 — 18 用例
- [x] UserService 注册/登录测试 — 53 用例
- [x] FriendService 全功能测试 (申请/同意/拒绝/删除/拉黑/列表) — 26 用例
- [x] MsgService 全功能测试 (发送/撤回/送达确认/已读确认) — 22 用例
- [x] ConvService 全功能测试 (列表/删除/免打扰/置顶/多用户隔离) — 23 用例
- [x] GroupService 全功能测试 (建群/解散/入群/退群/踢人/角色/更新) — 29 用例
- [x] FileService 全功能测试 (上传/下载/权限/共享会话鉴权) — 15 用例
- [x] AppHelper + Application 启动/数据库初始化测试 — 17 用例
- [x] WsGateway WebSocket 网关测试 — 6 用例

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
- [x] 多 msg_type 支持 (文本/表情/图片/语音/视频/文件/位置/名片) — UI 渲染 + 会话列表类型预览

### 会话服务 ✅
- [x] ConvService::GetConvList (会话列表 + 未读数 + 最后一条消息摘要 + 私聊对方昵称)
- [x] ConvService::DeleteConv (软隐藏会话, hidden=1, 新消息自动恢复)
- [x] ConvService::MuteConv (免打扰开关, mute 0/1)
- [x] ConvService::PinConv (置顶开关, pinned 0/1)
- [x] BroadcastConvUpdate (ConvUpdate 推送辅助函数)
- [x] MsgService 自动恢复隐藏会话 (发送消息后 unhide)
- [x] ConvUpdate 推送 (新消息摘要/成员变化/信息变更) — convUpdate 事件刷新会话列表

### 群组服务 ✅
- [x] GroupService::CreateGroup (建群 + 自动创建 Conversation + 封禁用户校验 + 成员上限 500)
- [x] GroupService::DismissGroup (解散群, 仅群主)
- [x] GroupService::JoinGroup / HandleJoinReq (入群申请 + 审批 + 封禁/删除用户校验 + 群员上限检查)
- [x] GroupService::LeaveGroup (退群, 群主须先转让)
- [x] GroupService::KickMember (踢出 + 权限校验)
- [x] GroupService::UpdateGroup (修改群名/头像/公告 + 头像长度校验 512)
- [x] GroupService::GetGroupInfo / GetMembers / GetMyGroups (批量查询优化)
- [x] GroupService::SetMemberRole (设置管理员)
- [x] GroupNotify 推送 (创建/解散/加入/退出/踢出/变更/角色)

### 文件服务 ✅
- [x] FileService::Upload (请求上传 → Insert + UpdatePath 防路径碰撞)
- [x] FileService::UploadComplete (上传完成确认)
- [x] FileService::Download (请求下载 + 共享会话成员鉴权)
- [x] 文件大小限制 (100MB)

### 同步服务 ✅
- [x] SyncService::SyncMessages (离线消息拉取, 分页 + has_more 标记)
- [x] SyncService::SyncUnread (未读会话 + 未读数 + 最后消息预览, 批量查询优化)

### 输入校验 ✅
- [x] 邮箱格式校验 + 长度限制 (255)
- [x] 密码长度校验 (6-128)
- [x] 昵称长度校验 (100) + 控制字符检测
- [x] 消息内容长度限制 (max_content_size 可配置)
- [x] 群名长度限制 (100) + 控制字符检测
- [x] 群公告长度限制 (1000)
- [x] 头像路径长度限制 (512)
- [x] 文件大小限制 (100MB)
- [x] 群成员上限 (500)

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
- [x] 权限混淆: Admin/User 表分离, admin_roles 独占管理员
- [x] 登录频率限制: RateLimiter 滑动窗口 (5次/60秒/IP, HTTP 429)
- [x] 密码内存清除: 验证后 volatile memset 清零明文
- [x] trust_proxy: X-Forwarded-For / X-Real-IP 仅配置启用时信任
- [x] 消息去重超时: in-flight 30s timeout 防 TOCTOU
- [x] ApiError 类型化: 28 个 constexpr 常量，消除 hardcode 字符串
- [x] NOVA_DEFER 宏: Go-style scope guard（事务回滚、资源清理）
- [x] Packet::Encode 校验: body 长度 ≤ kMaxBodySize
- [x] IsRevoked fail-closed: 查询失败视为已吐销
- [x] XFF IP 伪造修复: 取 X-Forwarded-For 最后一跳 (rightmost)
- [x] 负 seq 防护: SyncMessages seq 参数校验 ≥ 0
- [x] 封禁/删除用户校验: GroupService 入群/加群时校验用户状态
- [x] 群成员上限检查: CreateGroup/JoinGroup 不超过 500
- [x] 头像路径校验: UpdateGroup avatar 长度 ≤ 512
- [x] Admin middleware 状态检查: 排除 status=deleted 的管理员
- [x] 已删除用户占位: ListUsers 返回已删除好友的占位信息
- [x] 好友申请人校验: HandleFriendRequest 验证操作者为接收方
- [x] 批量 DAO 优化: GroupDao/ConvDao 批量查询消除 N+1
---

## 📚 文档

- [x] 数据库设计 docs/db_design.sql
- [x] Admin 需求文档
- [x] Admin API 设计
- [x] Admin DB 补充设计
- [x] Admin 实现计划
- [x] Admin 前端设计文档
- [x] 服务端架构文档
- [x] 协议文档
- [x] 系统架构文档
- [x] 软件设计文档 (含 Mermaid 图)
- [x] IM 服务器设计/API/DB 文档
- [x] SDK 文档 (API 参考 + 架构)
- [x] 桌面客户端文档 (UI 结构 + Bridge + 生命周期)
- [x] 脚本使用文档 (scripts/README.md)
- [x] API 文档 (协议文档 protocol.md 已完成 — OpenAPI 可选)
- [x] 部署指南 (configs/server.yaml + scripts/README.md 已包含)
- [x] 开发者快速开始指南 (README.md + scripts/README.md)

---

## � 开发计划总览

> **原则**: 框架搭建优先，具体业务实现跟进。先跑通最小闭环，再逐步补全功能。

### 里程碑路线图

| 阶段 | 目标 | 交付物 | 状态 |
|------|------|--------|------|
| **M1** | Admin 前端脚手架 | Vue 项目可运行，能登录 | ✅ 完成 |
| **M2** | IM 客户端 C++ 框架 | 共享库编译通过，能连接服务器 | ✅ 完成 |
| **M3** | PC 端 WebView2 | Win32+WebView2 可编译运行 | ✅ 完成 |
| **M4** | Admin 前端功能完善 | 全部管理页面可用 | ✅ 完成 |
| **M5** | IM 客户端业务实现 | 登录/聊天/联系人可用 | ✅ 完成 |
| **M6** | PC 端 UI 完善 | 完整桌面 IM 体验 | ✅ 完成 |
| **M7** | 移动端 Bridge | iOS/Android 可编译运行 | � 框架 |

---

## ✅ M1 — Admin 前端脚手架 (框架搭建) ✅

### 1.1 项目初始化
- [x] Vite + Vue 3 + TypeScript 项目创建 (server/web/)
- [x] ESLint + Prettier 配置 (可选 — vue-tsc 类型检查已集成)
- [x] Element Plus 集成 (按需导入 + unplugin-auto-import)
- [x] 目录结构搭建 (`api/` `views/` `layout/` `router/` `stores/` `utils/`)

### 1.2 基础框架
- [x] Axios 实例封装 (baseURL, 超时, 错误拦截)
- [x] Token 管理工具 (localStorage 存取)
- [x] Axios 请求拦截器 (自动注入 Authorization Bearer)
- [x] Axios 响应拦截器 (401 → 清 token → 跳登录, 403/429 处理)
- [x] Pinia auth store (token / adminInfo / permissions / hasPermission)
- [x] Vue Router 配置 (路由表 + 导航守卫 + 已登录跳转)
- [x] 主布局组件 (侧边栏 + 顶栏 + 内容区 + 面包屑 + 折叠)

### 1.3 登录闭环验证
- [x] 登录页面 (POST /auth/login)
- [x] 登录成功 → 存 token → fetchMe → 跳 dashboard
- [x] 路由守卫拦截未登录 → /login
- [x] 登出 (POST /auth/logout + 清 token)
- [x] 开发代理配置 (Vite proxy → :9091)

### 1.4 测试基础设施
- [x] Vitest + happy-dom 配置
- [x] Token 工具测试 (3 用例)
- [x] Auth Store 测试 (2 用例)
- [x] Router 配置测试 (2 用例)
- [x] Login 组件渲染测试 (1 用例)
- [x] 8/8 测试通过, vue-tsc 类型检查 0 errors

---

## ✅ M2 — IM 客户端 C++ 共享层框架 ✅

### 2.1 CMake 工程搭建
- [x] `client/nova_sdk/CMakeLists.txt` 重构 (编译为 shared library `nova_sdk`)
- [x] 依赖管理 (复用 protocol/, libhv, spdlog)
- [x] 导出头文件组织 (NOVA_SDK_API 宏, dllexport/dllimport)
- [x] 跨平台编译验证 (Windows MSVC ✅ / Linux GCC + macOS Clang 待验证)

### 2.2 Core 基础设施
- [x] 客户端日志 (spdlog 封装, 文件 + 控制台, 轮转)
- [x] 客户端配置 (server_host, server_port, 心跳, 重连, 超时)
- [x] EventBus (发布-订阅, 线程安全, 类型化事件)
- [x] UIDispatcher 接口 (回调投递到 UI 线程, 各平台实现)

### 2.3 Network 层
- [x] TcpClient 封装 (libhv TcpClient, connect/disconnect/send)
- [x] Codec (复用 protocol/Packet 编解码 + UNPACK_BY_LENGTH_FIELD)
- [x] ReconnectManager (指数退避: 1s→2s→4s→...→30s, 自动重连)
- [x] RequestManager (seq_id 请求-响应匹配, 超时回调)
- [x] ConnectionState 状态机 (Disconnected→Connecting→Connected→Authenticated→Reconnecting)

### 2.4 本地存储框架 (N/A — 客户端采用 WebView2 + Pinia Store 架构)
- [x] 状态管理通过 Pinia Store 实现 (替代本地 SQLite)
- [x] 数据通过 Bridge 从服务端实时同步

### 2.5 ViewModel 基础 (N/A — 采用 Vue Composition API + Pinia)
- [x] ViewModel 通过 Pinia defineStore 实现
- [x] 响应式属性通过 Vue ref/computed 实现
- [x] ClientContext (全局依赖注入: network, requests, reconnect, eventbus)

### 2.6 最小闭环验证 (已通过 WebView2 实现)
- [x] LoginViewModel — auth store login() + register() 通过 Bridge 调用 C++ SDK
- [x] 闭环验证: 登录 → 消息收发 → 好友/群组 → 文件 → 断线重连

### 2.7 单元测试 (14 用例)
- [x] EventBus 测试 (6 用例: 订阅/发布/多订阅/类型隔离/取消/清除)
- [x] RequestManager 测试 (6 用例: 匹配/未匹配/超时/取消/批量取消/多请求)
- [x] ConnectionState 测试 (2 用例: 枚举值/字符串转换)

---

## ## ✅ M3 — PC 端 WebView2 桌面客户端 (完成)

### 3.1 WebView2 项目搭建
- [x] `client/desktop/CMakeLists.txt` (WebView2 SDK 自动下载 + nova_sdk 链接, Windows-only)
- [x] main.cpp (Win32 wWinMain + COM 初始化 + ClientContext)
- [x] WebView2App (Win32 窗口类 + WebView2 异步初始化 + 虚拟主机映射)
- [x] Win32UIDispatcher (PostMessage WM_APP → UI 线程回调)

### 3.2 Web 前端框架
- [x] HTML/CSS/JS SPA 骨架 (index.html + style.css + JS 模块)
- [x] 主题系统 (CSS Variables — 颜色/字体/间距/圆角)
- [x] 页面路由 (NovaApp.navigate — login/main)
- [x] 全局消息提示组件 (Toast / Notification)

### 3.3 C++ ↔ JS 通信桥
- [x] JsBridge (WebView2 WebMessage 双向通信)
- [x] NovaBridge.js (JS 端 send/on/off API)
- [x] 登录请求 → LoginReq → LoginAck → JS 回调
- [x] 发送消息 → SendMsgReq → SendMsgAck → JS 回调
- [x] EventBus 订阅 (PushMsg/RecallNotify → JS 事件)

### 3.4 登录闭环
- [x] 登录页 (邮箱 + 密码 + 登录按钮 + 错误提示)
- [x] 登录成功 → 切换到主页面骨架
- [x] 主界面三栏布局骨架 (侧边栏 64px + 会话列表 280px + 聊天区)
- [x] 聊天区消息气泡 + 输入框 + 发送按钮
- [x] 连接状态条 (底部状态点 + 文字)
- [x] LoginViewModel 完整实现 (登录 + 注册 + 错误处理 + 超时 + 被踢处理 + 账号缓存)

---

✅ M4 — Admin 前端功能完善 ✅

### 4.1 仪表盘
- [x] 数据概览卡片 (连接数 / 在线用户 / 今日消息 / 运行时长)
- [x] 系统资源面板 (CPU / 内存 / 异常包数)
- [x] 统计图表 (可选 ECharts — 待实际数据量大时集成)

### 4.2 用户管理
- [x] 用户列表 (分页表格 + keyword/status 筛选)
- [x] 创建用户对话框 (邮箱 + 密码 + 昵称, 表单校验)
- [x] 用户详情抽屉 (在线状态 + 设备列表)
- [x] 操作按钮 (重置密码 / 封禁 / 解禁 / 踢出 / 删除, 带确认)

### 4.3 消息管理
- [x] 消息列表 (按会话 ID / 时间范围筛选, 分页)
- [x] 消息撤回对话框 (含 reason 输入)

### 4.4 审计日志
- [x] 日志列表 (按操作者 / 动作 / 时间筛选, 分页)

### 4.5 运维管理
- [x] 管理员列表 / 创建 / 删除 (AdminList.vue)
- [x] 角色列表 / 创建 / 编辑 / 删除 / 权限分配 (RoleList.vue)

---

## ✅ M5 — IM 客户端业务实现 (完成)

### 5.1 Model 层实现 (通过 Pinia Store 实现)
- [x] UserModel — auth store (uid/nickname/isLoggedIn)
- [x] MessageModel — chat store (messages[] + loadMoreHistory + syncMessages)
- [x] ConversationModel — chat store (conversations[] + unreadCount + pinned/mute)
- [x] ContactModel — contacts store (friends[] + friendRequests[] + search + block)
- [x] GroupModel — groups store (groups[] + currentGroupInfo + currentMembers)
- [x] FileModel — chat store (sendFileMessage: requestUpload → uploadComplete → sendMessage)

### 5.2 ViewModel 层实现 (Pinia Store)
- [x] LoginViewModel 完善 (注册 + 错误处理 + 超时 + 被踢通知)
- [x] ChatViewModel (消息收发 + 撤回 + 已读回执 + 历史加载 + 发送状态跟踪)
- [x] ConversationListViewModel (排序: 置顶→时间, 未读气泡, 免打扰标记)
- [x] ContactViewModel (好友列表 + 申请处理 + 搜索用户 + 拉黑/解除)
- [x] GroupViewModel (建群 + 解散 + 入群 + 踢人 + 角色管理 + 群信息编辑)
- [x] ProfileViewModel (个人资料查看/编辑)
- [x] SyncViewModel (离线消息同步 + 未读计数同步)

### 5.3 推送处理
- [x] 消息推送接收 + UI 通知 (newMessage 事件 + Toast)
- [x] 好友申请/同意/拒绝 推送处理 (friendNotify 事件)
- [x] 群通知推送处理 (groupNotify 事件 — 入群/退群/踢出/解散)
- [x] 会话更新推送 (convUpdate 事件 + 未读数同步)

---

## ✅ M6 — PC 端 UI 完善

### 6.1 会话模块
- [x] 会话列表组件 (头像 + 昵称 + 最后消息 + 未读数 + 时间 + 群聊区分)
- [x] 会话右键菜单 (置顶 / 免打扰 / 删除)
- [x] 会话搜索过滤

### 6.2 聊天模块
- [x] 聊天界面 (消息气泡 + 时间分割线 + 加载更多)
- [x] 输入框 (文本 + Emoji 选择器 + 文件发送按钮)
- [x] 消息状态指示 (发送中 / 已发送 / 发送失败)
- [x] 消息撤回 UI (右键菜单 + 撤回提示)
- [x] 消息复制 (右键菜单)

### 6.3 联系人模块
- [x] 好友列表 + 删除
- [x] 群列表 (群头像 + 成员数 + 角色)
- [x] 好友申请面板 (待处理数 badge + 接受/拒绝)
- [x] 添加好友 / 搜索用户

### 6.4 其他
- [x] 用户资料弹窗 (UserProfileModal — 头像/昵称/UID/操作按钮)
- [x] 群管理面板 (成员列表 + 角色标签 + 踢出/设管理 + 群信息编辑)
- [x] 设置页面 (账号昵称 / 通知开关 / 声音开关 / 版本信息)
- [x] 全局 Toast 通知组件 (success/error/warning/info + 自动消失)
- [x] 系统托盘 + 消息通知 (Windows — Win32 Shell_NotifyIcon + 气泡通知 + 任务栏闪烁)

---

## � M7 — 移动端 Bridge (框架已搭建)

### 7.1 iOS
- [x] CMake 构建脚本 (条件编译 — NOVA_BUILD_IOS)
- [x] Objective-C++ Bridge 层 (NovaClient.h/.mm — 完整公开接口)
- [x] 代理协议 (NovaClientDelegate — 状态变化/登录结果/消息推送)
- [ ] Xcode 项目 + CMake 交叉编译 (arm64)
- [ ] SwiftUI View 骨架 (登录 + 主页)

### 7.2 Android
- [x] CMake 构建脚本 (条件编译 — NOVA_BUILD_ANDROID)
- [x] JNI Bridge 层 (nova_jni.cpp — 完整 native 接口)
- [x] Kotlin 封装层 (NovaClient.kt — 单例 + Callback 接口)
- [ ] Gradle + CMake 交叉编译 (arm64-v8a, armeabi-v7a)
- [ ] Jetpack Compose View 骨架 (登录 + 主页)