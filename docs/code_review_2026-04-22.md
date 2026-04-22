# NovaIIM 全栈代码审查报告

日期：2026-04-22  
审查范围：后端 C++ (server/)、Admin 前端 Vue (server/web/)、Client SDK (client/nova_sdk/)、桌面 WebView2 (client/desktop/)、移动端 Bridge (client/mobile/)  
代码量：~14,000 LoC C++ + 1,200 LoC TS/Vue  
测试基线：294 后端 + 8 前端，全绿

---

## 总体评分

| 模块 | 评分 | 说明 |
|------|------|------|
| 后端 C++ (server/) | **B+** | 架构扎实，但有并发竞态和 RBAC 强制执行漏洞 |
| Admin 前端 (Vue) | **B** | 功能完整，但权限/CSRF/错误处理薄弱 |
| Client SDK + Desktop | **B-** | JsBridge 输入未校验，回调生命周期不安全 |
| 移动端 (iOS/Android) | **C+** | Android JNI 引用泄漏严重，iOS 缺空检查 |

---

## 🔴 严重问题 (Critical) — 必须修复

### 后端
1. **RBAC 路由级未实际强制** — [server/admin/admin_server.cpp](../server/admin/admin_server.cpp)  
   `AuthMiddleware` 注入 `X-Nova-Permissions` 头但没有 `RequirePermission()` 调用，任何已登录管理员可调用所有端点。
2. **conn_manager device_id TOCTOU** — [server/net/conn_manager.cpp](../server/net/conn_manager.cpp) L25  
   `device_id()` 在循环中多次调用，每次重新加锁。
3. **msg_service dedup 锁范围不足** — [server/service/msg_service.cpp](../server/service/msg_service.cpp) L110  
   dedup 锁仅覆盖检查阶段，DB 插入在锁外，重复消息可能写入。
4. **JWT admin_id=0 未拒绝** — [server/admin/admin_server.cpp](../server/admin/admin_server.cpp) L287  
   `claims->admin_id == 0` 未显式拒绝，依赖下游 FindById 防御。
5. **FileServer symlink + detach 风险** — [server/file/file_server.cpp](../server/file/file_server.cpp)  
   上传文件无 symlink 检查；大文件用 `std::thread::detach()`，关停时数据丢失。

### 客户端 SDK
6. **Android JNI 局部引用泄漏** — [client/mobile/android/nova_jni.cpp](../client/mobile/android/nova_jni.cpp) L93/L129/L145  
   `jclass cls = env->GetObjectClass(...)` 后未 `DeleteLocalRef(cls)`。约 500 条消息后 JNI 引用表溢出，应用崩溃。
7. **JsBridge 无输入校验** — [client/desktop/win/js_bridge.cpp](../client/desktop/win/js_bridge.cpp) L120  
   无 action 白名单/类型校验，JS 端可发送任意 JSON，类型不匹配时崩溃，存在 DoS 风险。
8. **RequestManager 断线不清理** — [client/nova_sdk/core/client_context.cpp](../client/nova_sdk/core/client_context.cpp) L89  
   `RequestManager::CancelAll()` 仅在 `Shutdown()` 时调用，断线时不清理 pending 请求，重连后旧 timeout 触发陈旧回调。

### 前端
9. **auth store 状态不一致** — [server/web/src/stores/auth.ts](../server/web/src/stores/auth.ts) L19  
   `login()` → `fetchMe()` 失败时 token 已写但 adminInfo 为 null，路由可能访问 null 数据。

---

## 🟠 高优先级 (High)

### 后端
- **dedup 单 mutex 瓶颈** — [server/service/msg_service.h](../server/service/msg_service.h) L33，高 QPS 下严重竞争，应分片
- **用户枚举漏洞** — [server/service/friend_service.cpp](../server/service/friend_service.cpp) L61，`BlockedByTarget` vs `UserNotFound` 错误信息不一致
- **FileServer 无 MIME 校验** — 任意文件可被上传并通过 /static 访问
- **每请求查权限** — admin_server 每个请求都查 DB 取权限，应缓存到 token 或内存

### SDK / 桌面
- **TcpClient 回调悬挂** — [client/nova_sdk/infra/tcp_client.cpp](../client/nova_sdk/infra/tcp_client.cpp) L75，libhv 回调 lambda 捕获 `this` 无生命周期保护
- **JsBridge UIDispatcher 捕获 this** — [client/desktop/win/js_bridge.cpp](../client/desktop/win/js_bridge.cpp) L107
- **iOS 空指针** — [client/mobile/ios/NovaClient.mm](../client/mobile/ios/NovaClient.mm) L57，`stringWithUTF8String:nullptr` 会崩溃
- **JNI Attach 不 Detach** — [client/mobile/android/nova_jni.cpp](../client/mobile/android/nova_jni.cpp) L50，线程退出时 JVM 资源泄漏

### 前端
- **401 拦截不清 store** — [server/web/src/utils/http.ts](../server/web/src/utils/http.ts) L45，仅清 localStorage 不清 Pinia 状态
- **多个 view 缺 try/catch** — UserList/AdminList/RoleList/MessageList 异步函数无错误处理
- **角色名→id 脆弱映射** — [server/web/src/views/admins/AdminList.vue](../server/web/src/views/admins/AdminList.vue) L195
- **密码强度仅 6 位** — 无大写/数字/特殊字符要求
- **删除/封禁按钮无 loading 锁** — 可双击重复提交

---

## 🟡 中等问题 (Medium)

### 后端
- `rate_limiter` purge 是 O(N) 每 1024 次写触发一次
- password 内存清理用 `volatile char*` 不规范，编译器可能优化掉
- snowflake 重启不持久化序列号（同毫秒重启理论冲突）
- 错误响应不一致（有的返回通用 kDatabaseError，有的具体）
- 消息广播按连接重新编码，应预编码一次复用

### SDK
- `ReconnectManager` 无 jitter（[reconnect_manager.cpp](../client/nova_sdk/core/reconnect_manager.cpp) L55），服务重启时雪崩风险
- `Win32UIDispatcher` 静态 HWND 非原子（[win32_ui_dispatcher.h](../client/desktop/win/win32_ui_dispatcher.h) L19）
- WebView2 缺 CSP 头（[webview2_app.cpp](../client/desktop/win/webview2_app.cpp) L196）
- WebView2 cleanup 顺序：bridge_ 先于 client_ shutdown，可能导致回调到死 bridge

### 前端
- 无 CSRF token；vite proxy 硬编码 URL
- Watcher 不清理（[RoleList.vue](../server/web/src/views/roles/RoleList.vue) L88）
- 日期 picker 无时区处理
- 查询参数无客户端校验（page=-1 等）
- 测试覆盖不足（仅 8 用例，未覆盖各 view）

---

## 🔵 低优先级 (Low)

- 无国际化 i18n
- 错误消息中英文混用
- 无前端用户行为日志
- 缺 `.env.example`
- 缺 ARIA 标签和键盘导航支持
- 部分 spdlog 日志级别偏低（DEBUG 应为 INFO）

---

## ✅ 优点

- ServerContext 依赖注入清晰
- ormpp 全参数化查询，无 SQL 注入风险
- PBKDF2-SHA256 100k iterations + JWT 黑名单 + 滑动窗口限流齐全
- DAO 模板支持 SQLite/MySQL 双后端，schema 一致
- 服务层批量查询消除 N+1
- 294 后端 + 8 前端测试全绿
- Snowflake ID 生成器优雅处理时钟回拨
- 前端 Vue 3 + TS + Element Plus 工程结构标准
- 桌面端通过 `alive_` weak_ptr 防御部分回调悬挂
- ConnManager 16 路 sharded lock 减少竞争
- MPMC Vyukov 无锁队列 + counting_semaphore 无虚假唤醒

---

## 测试覆盖盲点

- 并发场景：conn_manager TOCTOU、msg_service dedup
- RBAC 权限强制（路由级未测试）
- FileServer symlink 攻击 + 大文件关停
- DB 故障注入（只读、连接丢失）
- 时钟回拨与 Snowflake 重启
- 用户枚举攻击（好友拉黑/不存在账号差异）
- 客户端：JNI 引用计数压力测试、JsBridge 模糊测试

---

## 推荐修复顺序

| # | 任务 | 估时 | 影响 |
|---|------|------|------|
| 1 | 添加 `RequirePermission(perm)` 中间件并应用到所有路由 | 2h | 关闭权限旁路 |
| 2 | Android JNI 全部 `GetObjectClass` 后补 `DeleteLocalRef` | 1h | 阻止应用崩溃 |
| 3 | JsBridge 增加 action 白名单 + 类型校验 + try/catch | 3h | 阻止崩溃和注入 |
| 4 | conn_manager 缓存 device_id 一次再使用 | 30min | 消除 TOCTOU |
| 5 | msg_service dedup 锁覆盖至 DB 插入完成 | 1h | 消除消息重复 |
| 6 | RequestManager 在 disconnect 回调中清理 pending | 1h | 防陈旧回调 |
| 7 | TcpClient 用 weak_ptr/shared 包裹回调 | 2h | 防 use-after-free |
| 8 | 前端 auth store: fetchMe 失败时回滚 token | 30min | 修复状态不一致 |
| 9 | 前端各 view 补 try/catch + 删除按钮 loading 锁 | 2h | 健壮性 |
| 10 | ReconnectManager 加 jitter | 15min | 防雪崩 |
| 11 | 前端 401 拦截调用 authStore.doLogout() | 15min | 修复脏状态 |
| 12 | FileServer 加 symlink 检查 + 替换 detach 为 thread pool | 2h | 防穿越和数据丢失 |

预计总修复工时：~16h（关键路径 P0-P5 约 9h）

---

## 后续行动建议

1. **立即修复 P0-P3** (RBAC、JNI、JsBridge、conn_manager) — 涉及崩溃和权限旁路
2. **下个 Sprint 完成 P4-P9** — 健壮性与状态一致性
3. **CI 增强**：引入 ThreadSanitizer (TSAN)、`npm audit`、`cppcheck`
4. **文档**：补充权限矩阵文档（哪个端点要求哪个权限）
5. **压测**：JMeter/k6 模拟 1k 并发登录 + 10k 在线消息收发
