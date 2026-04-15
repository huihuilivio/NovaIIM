# NovaIIM Server 待办列表

## 基础设施
- [x] 实现 Gateway (libhv 网络层)
- [x] 添加配置文件加载 (ylt struct_yaml)
- [x] 集成 spdlog 日志系统
- [x] main.cpp 信号处理/优雅退出
- [x] ThreadPool 接入 Gateway 消息分发
- [x] MPMCQueue 完善 (move Push, 容量 assert)
- [ ] 协议序列化 (struct_pack → Packet body)

## IM 核心业务
- [ ] 实现 UserService 登录/登出
- [ ] 实现 MsgService 消息收发
- [ ] 实现 SyncService 离线同步
- [ ] 输入校验与错误处理

## 数据库
- [x] ormpp + SQLite3 集成 (WAL + FK)
- [x] DbManager 封装 (Open/Close/InitSchema)
- [x] Model 全部定义 (types.h, ormpp ADL 别名)
- [ ] MessageDaoImpl 实现
- [ ] ConversationDaoImpl 实现
- [ ] UserDeviceDaoImpl 实现

## Admin 管理面板

### Phase 0 — 基础工具 ✅
- [x] http_helper: JSON 响应 + 分页 + 权限检查
- [x] password_utils: PBKDF2-SHA256 (MbedTLS)
- [x] AdminConfig: jwt_secret / jwt_expires
- [x] server.yaml 更新

### Phase 1 — DAO 层 ✅
- [x] UserDaoImpl (参数化查询, LIKE 转义)
- [x] AuditLogDaoImpl (多条件参数化分页)
- [x] AdminSessionDaoImpl (JWT 黑名单)
- [x] RbacDaoImpl (3表 JOIN 权限查询)
- [x] main.cpp 集成 DbManager

### Phase 2 — 认证 + 鉴权 ← 当前
- [x] JwtUtils (l8w8jwt Sign/Verify)
- [x] AuthMiddleware JWT 验签 + X-Nova-* 防伪造
- [ ] AuthMiddleware 补全: 黑名单查询 + permissions 注入
- [ ] POST /auth/login
- [ ] POST /auth/logout
- [ ] GET /auth/me

### Phase 3 — 业务 API
- [ ] GET /dashboard/stats (已有逻辑，需加权限)
- [ ] GET/POST /users (列表/创建)
- [ ] GET/DELETE /users/:id (详情/删除)
- [ ] POST /users/:id/reset-password
- [ ] POST /users/:id/ban / unban / kick
- [ ] GET /messages (消息查询)
- [ ] POST /messages/:id/recall (撤回)

### Phase 4 — 审计 + 测试
- [ ] GET /audit-logs
- [ ] 路由注册重构 + DAO 注入
- [ ] JWT / PasswordUtils 单元测试
- [ ] Handler / 集成测试

## 安全加固 ✅
- [x] SQL 注入: 全参数化 (ormpp prepared statement)
- [x] 请求头伪造: AuthMiddleware 清除 X-Nova-*
- [x] UID 欺骗: Heartbeat 用 conn->user_id()
- [x] 数据竞争: user_id_ atomic, device_id_ mutex
- [x] 密码安全: PBKDF2 100k iterations, mbedtls 返回值检查
- [x] 配置安全: JWT 密钥启动校验
- [x] LIKE 注入: 通配符转义 + ESCAPE
- [x] 整数溢出: Pagination::Offset() → int64_t

## 文档
- [x] 数据库设计 docs/db_design.sql
- [x] Admin 需求文档
- [x] Admin API 设计
- [x] Admin DB 补充设计
- [x] Admin 实现计划
- [x] 服务端架构文档
- [x] 协议文档
- [x] 系统架构文档