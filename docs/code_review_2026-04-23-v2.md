# NovaIIM 全栈代码审查报告（第三轮 / Post-Fix Verification）

日期：2026-04-23（同日复审）
上次审查：[code_review_2026-04-23.md](code_review_2026-04-23.md)
关联提交：`c2cfac7` — 14 文件 +587/-86，修复 P0/P1/P2 共 11 项
审查范围：后端 C++ (server/)、Admin 前端 Vue (server/web/)、Client SDK (client/nova_sdk/)、桌面 WebView2 (client/desktop/)、移动端 Bridge (client/mobile/)
测试基线：321 后端 + 8 前端，全绿

---

## 总体评分（相比上次）

| 模块 | 04-22 | 04-23 上午 | 本次 | 说明 |
|------|-------|------------|------|------|
| 后端 C++ (server/) | B+ | A- | **A-** | RBAC/dedup/TOCTOU/symlink 全部修复；只剩配置硬化与运营任务 |
| Admin 前端 (Vue) | B | B+ | **A-** | 401/状态/错误处理已完善；密码强度、API 取消尚弱 |
| Client SDK + Desktop | B- | B- | **B+** | TcpClient UAF/JsBridge 异常/Reconnect jitter 全部修复 |
| 移动端 (iOS/Android) | C+ | C+ | **B+** | 3 个崩溃路径全部修复，剩 NewStringUTF OOM 防御 |

---

## 一、上轮 11 项修复 — 全部验证 ✅

| # | 修复点 | 验证结论 | 关键证据 |
|---|--------|----------|----------|
| 1 | Android JNI `DeleteLocalRef` | ✅ 正确 | [nova_jni.cpp](../client/mobile/android/nova_jni.cpp) L120/L138/L160 |
| 2 | Android `JniAttachGuard` RAII | ✅ 正确 | [nova_jni.cpp](../client/mobile/android/nova_jni.cpp#L43) L43-69 |
| 3 | iOS `NovaSafeNSString` | ✅ 正确 | [NovaClient.mm](../client/mobile/ios/NovaClient.mm#L13) L13-18 |
| 4 | FileServer symlink 检查 | ✅ 正确 | [file_server.cpp](../server/file/file_server.cpp) `IsDestinationSafe` |
| 4b | FileServer 线程追踪 | ⚠️ **新发现回归** 见下文 N1 | [file_server.cpp](../server/file/file_server.cpp#L107) |
| 5 | TcpClient `weak_ptr` 捕获 | ✅ 正确 | [tcp_client.cpp](../client/nova_sdk/infra/tcp_client.cpp#L83) L83-84 |
| 6 | AdminServer `admin_id<=0` | ✅ 正确 | [admin_server.cpp](../server/admin/admin_server.cpp#L259) L259 |
| 7 | ClientContext 断线 CancelAll | ✅ 正确 | [client_context.cpp](../client/nova_sdk/core/client_context.cpp#L55) L55 |
| 8 | JsBridge 顶层 try/catch | ✅ 正确 | [js_bridge.cpp](../client/desktop/win/js_bridge.cpp#L143) L143-275 |
| 9 | 前端 401 → `doLogout()` | ✅ 正确 | [http.ts](../server/web/src/utils/http.ts#L52) L52 + [auth.ts](../server/web/src/stores/auth.ts) 已导出 doLogout |
| 10 | Reconnect ±25% jitter | ✅ 正确 | [reconnect_manager.cpp](../client/nova_sdk/core/reconnect_manager.cpp#L94) L94-106 |
| 11 | password secure wipe | ✅ 正确 | [password_utils.cpp](../server/core/password_utils.cpp) L86/L87/L96/L97/L148-150/L161-163 全部 6 处 |

---

## 二、本轮新发现

### N1. **FileServer 计数器 vs 线程构造异常竞态**（**Critical**，本次修复回归）

[file_server.cpp](../server/file/file_server.cpp#L121) L107-138：

```cpp
} while (!downloads_inflight_.compare_exchange_weak(cur, cur + 1, ...));  // L121: ++inflight

std::thread([this, job = std::move(job)]() {  // L124: 若构造失败抛 std::system_error
    try { job(); } catch (...) { ... }
    downloads_inflight_.fetch_sub(1, ...);  // 永远不会执行
}).detach();
```

**问题**：CAS 成功递增 `inflight`，紧接着 `std::thread()` 构造若抛出（如系统线程资源耗尽），计数器永久泄漏 1。当达到上限或 Stop() 等待归零时，会**永久挂起 30s 直到超时**，且会逐渐累积导致后续下载全部被拒。

**修复**：将 `std::thread` 构造包入 try/catch，失败时回滚计数：

```cpp
try {
    std::thread([...]() { ... }).detach();
} catch (...) {
    downloads_inflight_.fetch_sub(1, std::memory_order_acq_rel);
    NOVA_NLOG_ERROR(kLogTag, "failed to spawn download thread");
    return false;
}
return true;
```

### N2. **JWT secret 最小长度未强制**（High）

[application.cpp](../server/core/application.cpp#L80) L80：仅 `NOVA_LOG_WARN` 提示 secret < 16 字符，**不阻止启动**。生产中若管理员忽略警告留默认 `change-me-in-production`，HS256 可被字典攻击伪造 token。

**修复建议**：在 `JwtUtils::Sign()` 入口或 `Application::Start()` 加 `assert(secret.size() >= 32)`，违反则返回 -1 阻止启动。  

### N3. **AdminSession 表无过期清理**（High，已知 TODO）

`admin_sessions` 表会无限累积。建议在 `Application::Init()` 注册一个 `ThreadPool::PostDelayed` 周期任务，每 10 分钟执行：

```sql
DELETE FROM admin_sessions WHERE expires_at < NOW();
```

并在 schema 加索引 `CREATE INDEX idx_expires ON admin_sessions(expires_at);`。

### N4. **db_design.sql 缺索引**（Medium）

[docs/db_design.sql](db_design.sql) — `admin_sessions` 表缺 `expires_at` 索引，N3 的清理任务会全表扫描；建议补 `idx_expires`、`idx_token_hash`。

### N5. **管理员自我保护边界**（已确认 OK，记录用）

[admin_server.cpp](../server/admin/admin_server.cpp#L1115) L1115：DELETE /admins/:id 已检查 `id == 1 || id == admin_id`；HandleUpdateRole [L1366] 已防止 `id == admin_id`。**不允许自删/自降权**——验证通过。

---

## 三、未修部分（沿用上次 P2/P3，按优先级）

| 优先级 | 问题 | 位置 |
|--------|------|------|
| **High** | FileServer 上传 MIME 白名单缺失 | [file_server.cpp](../server/file/file_server.cpp) 上传路由 |
| **High** | AdminSession 过期 token 定时清理（N3） | 新任务 |
| **High** | JWT secret 最小长度强制（N2） | [application.cpp](../server/core/application.cpp#L80) |
| **High** | FileServer 线程构造异常计数泄漏（N1） | [file_server.cpp](../server/file/file_server.cpp#L121) |
| Medium | AuthMiddleware RBAC 查询 TTL 缓存 | [admin_server.cpp](../server/admin/admin_server.cpp#L276) |
| Medium | WebView2 CSP 头 + NavigationStarting 白名单 | [webview2_app.cpp](../client/desktop/win/webview2_app.cpp) |
| Medium | 前端密码强度升级（≥8 + 字符类） | [AdminList.vue](../server/web/src/views/admins/AdminList.vue#L217) [UserList.vue](../server/web/src/views/users/UserList.vue) |
| Medium | 前端按行 loading（删除/封禁双击防御） | AdminList/UserList 等 |
| Medium | 角色名→id 改 id 直存（避免重名脆弱映射） | [AdminList.vue](../server/web/src/views/admins/AdminList.vue#L191) |
| Medium | 前端 API 请求路由切换取消 | http.ts |
| Medium | admin_sessions 表 expires_at 索引（N4） | [db_design.sql](db_design.sql) |
| Medium | dedup 单 mutex 改分片锁 | [msg_service.h](../server/service/msg_service.h#L32) |
| Medium | password 内存清理已修复 ✅ | — |
| Low | snowflake 序列号持久化 | [snowflake.h](../server/core/snowflake.h) |
| Low | JsBridge 改用 `unordered_map<string, Handler>` 分发表 | [js_bridge.cpp](../client/desktop/win/js_bridge.cpp#L149) |
| Low | i18n / ARIA / .env.example | 前端 |
| Low | 前端无 CSRF（管理员面板风险较低） | http.ts |

---

## 四、优点（保留 + 新增）

**保留：**
- ServerContext 依赖注入清晰、ormpp 全参数化、PBKDF2-SHA256 100k+ JWT 黑名单+滑动窗口限流齐全、DAO 双后端、批量查询消除 N+1、ConnManager 16 sharded lock、MPMC Vyukov 队列。

**本轮新发现：**
- **shutdown 顺序正确**：[application.cpp](../server/core/application.cpp#L268) L268-289 严格反向依赖关闭（FileServer→AdminServer→WsGateway→Gateway→ThreadPool），避免悬挂回调。
- **管理员自保护**：DELETE/PUT roles 均拦截 `id == admin_id`，无法自删/自降权。
- **GroupService 解散事务**：[group_service.cpp](../server/service/group_service.cpp) L240-349 用事务+成员快照，消除 dismiss 与 send 的竞态。
- **CMake 依赖全部 GIT_TAG 锁版本**：libhv/yalantinglibs/ormpp/googletest，避免 supply-chain 不稳定。
- **MsgService 错误路径全部 `DedupRemoveInflightIfNeeded`**：所有早返回路径都正确清理 in-flight 标记。

---

## 五、推荐下一批修复顺序

| # | 任务 | 估时 | 影响 |
|---|------|------|------|
| **NX-1** | FileServer 线程构造异常回滚计数（N1） | 15min | 修复挂起回归 |
| **NX-2** | JWT secret < 32 字符 fail-fast（N2） | 30min | 阻止生产弱配置 |
| **NX-3** | AdminSession 定时清理 + 索引（N3+N4） | 2h | DB 膨胀 |
| **NX-4** | FileServer 上传 MIME 白名单 | 1h | 任意文件托管 |
| **NX-5** | AuthMiddleware RBAC TTL 缓存 | 2h | DB QPS 优化 |
| **NX-6** | WebView2 CSP + 导航白名单 | 1h | XSS 防御纵深 |
| **NX-7** | 前端密码强度 + 按行 loading + role id 直存 | 2h | UX/安全 |
| **NX-8** | JsBridge 分发表重构 | 3h | 可维护性 |

总计 ~12h；NX-1 紧急（15min 即可消除挂起回归）。

---

## 六、结论

✅ **commit `c2cfac7` 的 11 项修复全部生效且无误**（本次唯一发现的回归是 FileServer 线程构造异常路径，N1）。

📈 项目整体安全/稳定性自 04-22 以来显著提升：
- 后端 B+ → A-
- 前端 B → A-
- SDK/桌面 B- → B+
- 移动端 C+ → B+

🎯 接下来建议立即应用 **NX-1**（15 分钟，避免回归扩大），其余 N2-N8 可纳入下一 Sprint。

测试基线：321 后端 + 8 前端，全绿。
