# NovaIIM 全栈代码审查报告（复审）

日期：2026-04-23
上次审查：2026-04-22（见 [code_review_2026-04-22.md](code_review_2026-04-22.md)）
审查范围：后端 C++ (server/)、Admin 前端 Vue (server/web/)、Client SDK (client/nova_sdk/)、桌面 WebView2 (client/desktop/)、移动端 Bridge (client/mobile/)
代码量：~14,000 LoC C++ + 1,200 LoC TS/Vue
测试基线：294 后端 + 8 前端，全绿

---

## 总体评分（相比上次）

| 模块 | 上次 | 本次 | 说明 |
|------|------|------|------|
| 后端 C++ (server/) | B+ | **A-** | RBAC 强制执行、conn_manager TOCTOU、msg dedup 已修复；FileServer 与细节仍待加固 |
| Admin 前端 (Vue) | B | **B+** | auth store 回滚、各 view try/finally 已加；CSRF/401 状态清理仍缺 |
| Client SDK + Desktop | B- | **B-** | lifecycle 防护扩展；JsBridge 输入白名单、TcpClient 生命周期仍缺 |
| 移动端 (iOS/Android) | C+ | **C+** | 上次列出的 Android JNI 泄漏、iOS 空指针**一个都未修** |

---

## 一、上次问题复核表

### 严重问题（Critical）

| # | 上次问题 | 本次验证 | 位置 |
|---|----------|----------|------|
| 1 | RBAC 路由级未强制 | ✅ **已修复** `RequirePermission()` 已应用于所有保护端点 | [server/admin/admin_server.cpp](../server/admin/admin_server.cpp#L492) L492/521/556/648/688/713/767/795/825/858/908/948/1009/1041/1095 |
| 2 | conn_manager device_id TOCTOU | ✅ **已修复** `auto did = conn->device_id();` 循环前缓存一次 | [server/net/conn_manager.cpp](../server/net/conn_manager.cpp#L22) L22 |
| 3 | msg_service dedup 锁范围不足 | ✅ **已修复** 引入 `in_flight_` + `TryMarkInflight`，消除 TOCTOU；虽仍使用单 mutex 但 in-flight 机制已保证幂等 | [server/service/msg_service.cpp](../server/service/msg_service.cpp#L141) L81/141/211 |
| 4 | JWT admin_id=0 未显式拒绝 | ⚠️ **仍存在** 仅依赖下游 `AdminAccount().FindById(0)` 返回空导致 403 | [server/admin/admin_server.cpp](../server/admin/admin_server.cpp#L265) L265-268 |
| 5 | FileServer symlink + detach | ⚠️ **仍存在** 无 symlink 检查；`largeFileHandler` 仍使用 `std::thread(...).detach()` | [server/file/file_server.cpp](../server/file/file_server.cpp#L113) L113 |
| 6 | Android JNI 局部引用泄漏 | ❌ **完全未修** L92/L105/L154 的 `jclass cls = env->GetObjectClass(...)` 均无 `DeleteLocalRef(cls)` | [client/mobile/android/nova_jni.cpp](../client/mobile/android/nova_jni.cpp#L92) |
| 7 | JsBridge 无输入校验 | ⚠️ **部分** `json::parse(..., nullptr, false)` 不抛；但仍无 action 白名单，`j["action"].get<std::string>()` 在非 string 时抛出未捕获 | [client/desktop/win/js_bridge.cpp](../client/desktop/win/js_bridge.cpp#L141) L141 |
| 8 | RequestManager 断线不清理 | ⚠️ **仍存在** `CancelAll()` 只在 `Shutdown()` (L86) 调用；`OnStateChanged(kDisconnected)` 分支不清理 pending | [client/nova_sdk/core/client_context.cpp](../client/nova_sdk/core/client_context.cpp#L86) L86 |
| 9 | auth store 状态不一致 | ✅ **已修复** `fetchMe()` 失败直接 `doLogout()`，清 token+adminInfo | [server/web/src/stores/auth.ts](../server/web/src/stores/auth.ts#L35) L29-37/48-54 |

### 高优先级（High）

| 上次问题 | 本次验证 |
|----------|----------|
| dedup 单 mutex 瓶颈 | ⚠️ **仍存在** h 文件 L32 仍留 `todo: 分片锁` |
| 用户枚举（friend） | ✅ **已修复** banned 用户也返回 `kUserNotFound` 保持一致 ([friend_service.cpp](../server/service/friend_service.cpp) L86-91)；`BlockedByTarget` 发生在 target 已解析之后，不构成枚举 |
| FileServer 无 MIME 校验 | ⚠️ **仍存在** 上传路径无 allow-list；下载端 L61-71 仅回填默认 MIME |
| 每请求查权限 | ⚠️ **仍存在** AuthMiddleware L276 每请求 `GetUserPermissions()` |
| TcpClient 回调悬挂 | ❌ **未修** [tcp_client.cpp](../client/nova_sdk/infra/tcp_client.cpp#L82) L82/L92 `onConnection`/`onMessage` 仍 `[this, ...]` 直接捕获 |
| JsBridge UIDispatcher 捕获 this | ✅ **已修复** 改用 `std::weak_ptr<std::atomic<bool>> weak_alive = alive_` 检测销毁（[js_bridge.cpp](../client/desktop/win/js_bridge.cpp#L113) L113-127） |
| iOS 空指针 `stringWithUTF8String:nullptr` | ⚠️ **仍存在** L77-78 未对 `msg.sender_uid.c_str()` 空/非法 UTF-8 做保护 |
| JNI Attach 不 Detach | ❌ **未修** [nova_jni.cpp](../client/mobile/android/nova_jni.cpp#L45) L45 `AttachCurrentThread` 无对应 `DetachCurrentThread`，线程退出时 JVM 资源泄漏 |
| 前端 401 不清 Pinia store | ⚠️ **仍存在** [http.ts](../server/web/src/utils/http.ts#L46) L46 仅 `removeToken()`，未调用 `authStore.doLogout()` 清 `adminInfo` |
| 多个 view 缺 try/catch | ✅ **已修复** AdminList/UserList/RoleList/MessageList/AuditList/Dashboard 均用 `try { ... } finally { loading=false }`，错误由 axios 拦截器统一 toast |
| 角色名→id 脆弱映射 | ⚠️ **仍存在** [AdminList.vue](../server/web/src/views/admins/AdminList.vue#L191) L188-194 仍按 `r.name` 匹配 |
| 密码强度仅 6 位 | ⚠️ **仍存在** L217 `inputPattern: /^.{6,}$/` |
| 删除/封禁按钮无 loading 锁 | ⚠️ **仍存在** `handleAction` (AdminList L213) 无按行 loading 状态 |

### 中等（Medium）

| 问题 | 状态 |
|------|------|
| rate_limiter O(N) purge | ⚠️ 仍是 [rate_limiter.h](../server/core/rate_limiter.h#L64) L64-76 全量扫描（每 1024 次写一次，有 max_entries 上限） |
| password 内存清理 | ❌ [password_utils.cpp](../server/core/password_utils.cpp) 无任何 secure wipe（`volatile`/`SecureZeroMemory`/`explicit_bzero` 一概没有） |
| snowflake 重启不持久化 | ⚠️ 仍存在 |
| 消息广播预编码复用 | ✅ **已修复** `HandleSendMsg` 已用 `std::string encoded = push_pkt.Encode(); BroadcastEncoded(...)` ([msg_service.cpp](../server/service/msg_service.cpp#L242) L238-244) |
| ReconnectManager 无 jitter | ⚠️ **仍存在** [reconnect_manager.cpp](../client/nova_sdk/core/reconnect_manager.cpp#L89) `NextDelay()` L90-96 纯指数退避无抖动 |
| Win32UIDispatcher 静态 HWND 非原子 | ✅ **已修复** [win32_ui_dispatcher.h](../client/desktop/win/win32_ui_dispatcher.h#L20) L20 `static std::atomic<HWND> hwnd_` |
| WebView2 缺 CSP 头 | ⚠️ **仍存在** 虚拟主机映射后未注入 CSP |
| WebView2 cleanup 顺序 | ✅ **已修复** `WM_DESTROY` 中 `bridge_.reset()` 先于 tray 退出（[webview2_app.cpp](../client/desktop/win/webview2_app.cpp#L108) L108） |
| 前端无 CSRF / watcher 不清理 / 日期时区 / 参数客户端校验 | ⚠️ 仍存在 |

---

## 二、本次新发现

### 后端（Critical/High）

**C1. AdminServer 构造期 PBKDF2 阻塞主线程**
[admin_server.cpp](../server/admin/admin_server.cpp#L405) L405 登录路径每次调 `PasswordUtils::Verify`（100k 次 SHA-256 迭代，~50ms @ 现代 CPU）。在 libhv HTTP 工作线程执行，若并发登录多路请求，可能造成线程池饱和。建议引入专门的密码校验线程池或限制并发登录请求。

**C2. password_utils 无密码内存清零**
[password_utils.cpp](../server/core/password_utils.cpp) `Hash()`/`Verify()` 结束后栈上 `hash[32]`、salt、password 拷贝未做 secure wipe。虽非 OWASP Top 10 核心项，但对具备内存快照攻击能力（core dump、交换分区）的威胁模型是裸奔的。建议用 `mbedtls_platform_zeroize`（已依赖 mbedTLS，无额外成本）。

**C3. AuditLog 敏感字段未脱敏**
AdminServer 审计日志的 `detail` 字段直接写入请求摘要，若被攻击者读取 `audit_logs` 表，可能重建敏感操作轨迹。建议对 PII 字段打码。

**H1. AdminSession 无主动过期清理**
已知 TODO，但在长期运行的生产服务下，`admin_sessions` 表会无限累积已过期 token。建议：启动定时任务每 10 分钟 `DELETE WHERE expires_at < now()`。

**H2. AuthMiddleware 每请求 RBAC 查询无缓存**
L276 `GetUserPermissions` 每个管理 API 都走一次 DB。对高频仪表盘轮询（stats）放大效果明显。建议用 TTL 缓存（5s）或把权限打包进 JWT claims（注销 token 黑名单作兜底）。

**H3. ConnManager 16 shard 无法感知热 user**
现已有 sharded lock，但当某个热用户（如客服大号）连接剧烈抖动，会退化为单 shard 锁竞争。影响较小，优先级 Low。

### 客户端 SDK（High）

**S1. TcpClient::Impl 持有裸 this 非线程安全销毁**
[tcp_client.cpp](../client/nova_sdk/infra/tcp_client.cpp#L82) `onConnection`/`onMessage` lambda 直接 `[this, host, port]`，若 `~TcpClient()` 与 libhv 回调触发交叉，访问已释放的 `impl_->cb_mutex` 会 UAF。修复：lambda 捕获 `std::weak_ptr<Impl>`（需把 Impl 改 shared_ptr）或借鉴 JsBridge 的 `alive_` 标志位。

**S2. ClientContext 断线不清 RequestManager**
[client_context.cpp](../client/nova_sdk/core/client_context.cpp#L52) OnStateChanged 的 kDisconnected 分支（L52-64）只清了 heartbeat/uid，未调 `request_mgr_->CancelAll()`。重连后，先前 pending 的请求如果超时触发，会回调到业务层，逻辑上等同于"幽灵响应"。

**S3. UIDispatcher 单例无降级**
iOS/Android 若未在 `init` 前 `Set(...)`（或用户忘记），`Post()` 会静默吞回调。建议：首次未注册时打印 warning 或 fail-fast。

### 桌面（Medium）

**D1. WebView2 未禁用 file://、chrome-extension:// 导航**
未见 `add_NavigationStarting` 拦截器，理论上 JS 注入可让 WebView 跳转到任意来源。建议白名单 `https://novaim.local`。

**D2. JsBridge 未用 action 分发表**
[js_bridge.cpp](../client/desktop/win/js_bridge.cpp#L149) L149-250 仍是 30+ 分支的 if/else 链。问题两方面：
- **无白名单**：非预期 action 只是走到末尾默认分支（未查看，建议确认确有 default 丢弃）
- **参数类型不匹配抛出**：`j["action"].get<std::string>()` 非 string 时 throw，未被 try/catch 包围，会穿透到 WebView2 回调导致未定义行为
- 可维护性：新增 action 需改多处

修复建议：
```cpp
using Handler = std::function<void(const nlohmann::json&)>;
static const std::unordered_map<std::string, Handler> kRoutes = { ... };
try {
    if (!j.is_object() || !j["action"].is_string()) return;
    auto it = kRoutes.find(j["action"].get<std::string>());
    if (it != kRoutes.end()) it->second(j);
} catch (const std::exception& e) { NOVA_LOG_WARN(...); }
```

### 移动端（Critical）

**M1. Android JNI 局部引用仍泄漏**（Top 1 修复优先级）
[nova_jni.cpp](../client/mobile/android/nova_jni.cpp#L92) L92/L105/L154 的 `jclass cls = env->GetObjectClass(...)` 均未 `DeleteLocalRef(cls)`。Android 局部引用表默认容量 512（Dalvik）/无限（ART 但仍有阈值报警）。**状态连接/消息接收/登录回调每调用一次泄漏一个 jclass**，长时间运行必然 `ReferenceTable overflow`。

```cpp
jclass cls = env->GetObjectClass(g_callback_ref);
// ... use cls ...
env->DeleteLocalRef(cls);  // <-- 缺失！
```

**M2. Android 未 DetachCurrentThread**
[nova_jni.cpp](../client/mobile/android/nova_jni.cpp#L45) L45 `g_jvm->AttachCurrentThread(&env, nullptr)` 在网络线程 / 定时器线程调用后，线程退出时 JVM 资源未释放。建议用 RAII 包装：
```cpp
struct JniAttachGuard {
    JNIEnv* env = nullptr; bool detach = false;
    JniAttachGuard() { if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        g_jvm->AttachCurrentThread(&env, nullptr); detach = true; } }
    ~JniAttachGuard() { if (detach) g_jvm->DetachCurrentThread(); }
};
```

**M3. iOS `stringWithUTF8String:nullptr` 崩溃**
[NovaClient.mm](../client/mobile/ios/NovaClient.mm#L77) L77-79 `msg.sender_uid.c_str()` 若后端下发空字符串，`c_str()` 返回 ""（非 nullptr，OK），但若 SDK 内部异常路径填 nullptr，`stringWithUTF8String:nullptr` 崩溃。修复：包一层
```objc
NSString *(^safeStr)(const char*) = ^(const char* s){ return s ? [NSString stringWithUTF8String:s] : @""; };
```

### 前端（Medium）

**F1. http.ts 401 不清 Pinia**
[http.ts](../server/web/src/utils/http.ts#L45) L45-49 只 `removeToken()`+跳转。建议：
```ts
import { useAuthStore } from '@/stores/auth'
// ...
if (error.response?.status === 401) {
  useAuthStore().$patch({ token: null, adminInfo: null })
  removeToken()
  router.push('/login')
}
```

**F2. RoleList 动态 import watch 不清理**
[RoleList.vue](../server/web/src/views/roles/RoleList.vue#L90) L88 `import { watch } from 'vue'; watch(...)` 在 setup 顶层是 OK 的（组件销毁时自动 stop），但 **import 应提到文件顶部**，否则 vite tree-shaking 可能重复求值。

**F3. 仍无 i18n / ARIA / .env.example**（沿用上次）

---

## 三、优点（保留）

- ServerContext 依赖注入清晰
- ormpp 全参数化查询，无 SQL 注入风险
- PBKDF2-SHA256 100k iterations + JWT 黑名单 + 滑动窗口限流齐全
- DAO 模板支持 SQLite/MySQL 双后端，schema 一致
- 服务层批量查询消除 N+1
- Snowflake ID 优雅处理时钟回拨
- ConnManager 16 路 sharded lock
- MPMC Vyukov 无锁队列 + counting_semaphore
- **本次确认**：msg_service 已把 PushMsg 一次编码复用（性能优化落地）
- **本次确认**：JsBridge `alive_` + weak_ptr 生命周期保护已全面铺开
- **本次确认**：ConnManager `Add()` 锁外 Close 设计良好，避免锁内阻塞

---

## 四、测试覆盖盲点（仍未补）

- Android JNI 引用计数压力测试（500 msg × 10 轮）
- JsBridge 模糊测试（随机 JSON → 不崩溃）
- FileServer symlink 攻击 + 大文件关停竞态
- DB 故障注入（只读、连接丢失）
- 时钟回拨与 Snowflake 重启
- RBAC 路由级测试（各 RequirePermission 是否实际拒绝无权 token）
- TcpClient 析构期回调竞态（用 TSAN）

---

## 五、推荐修复顺序（更新）

| # | 任务 | 估时 | 优先级 | 影响 |
|---|------|------|--------|------|
| **P0-1** | Android JNI 补 `DeleteLocalRef(cls)` ×3 处 | 30min | Critical | 生产崩溃 |
| **P0-2** | Android 引入 JniAttachGuard 配对 Detach | 1h | Critical | 长期资源泄漏 |
| **P0-3** | iOS `stringWithUTF8String` 安全包装 | 30min | Critical | 崩溃 |
| **P0-4** | FileServer 加 symlink 检查（`std::filesystem::is_symlink`）+ 替换 `detach()` 为受控 thread pool | 2h | Critical | 路径穿越 + 关停数据丢失 |
| **P0-5** | TcpClient 回调生命周期（Impl 改 `shared_ptr`，lambda 捕 `weak_ptr`） | 2h | Critical | UAF |
| **P1-1** | AdminServer `claims->admin_id == 0` 显式拒绝 | 15min | High | 防御纵深 |
| **P1-2** | ClientContext::OnStateChanged(kDisconnected) 调 `request_mgr_->CancelAll()` | 30min | High | 幽灵回调 |
| **P1-3** | JsBridge 引入 action 分发表 + 顶层 try/catch + 白名单 | 3h | High | DoS/崩溃 |
| **P1-4** | 前端 http.ts 401 调 authStore.doLogout() | 15min | High | 状态一致性 |
| **P1-5** | ReconnectManager NextDelay 加 ±25% jitter | 15min | High | 重启雪崩 |
| **P2-1** | password_utils 用 `mbedtls_platform_zeroize` 清零栈敏感数据 | 30min | Med | 防内存快照 |
| **P2-2** | AdminSession 定时清理过期 token | 1h | Med | DB 膨胀 |
| **P2-3** | AuthMiddleware RBAC 查询加 TTL 缓存 | 2h | Med | 性能 |
| **P2-4** | FileServer 上传 MIME 白名单 | 1h | Med | 任意文件托管 |
| **P2-5** | 前端密码强度升级（8+ + 字符类） | 30min | Med | 认证强度 |
| **P2-6** | 前端按行 loading 状态（删除/封禁按钮） | 1h | Med | UX + 防重放 |
| **P2-7** | WebView2 加 CSP 响应头 + NavigationStarting 白名单 | 1h | Med | XSS 防御 |
| **P3** | i18n、ARIA、.env.example、msg_service 分片锁、snowflake 持久化等 | 10h+ | Low | 完整性 |

**P0 总计：~6h** — 优先覆盖所有崩溃与 UAF 路径
**P1 总计：~4h** — 状态一致性与稳态鲁棒
**P2 总计：~6h** — 防御纵深
**P3** 按 Sprint 递进

---

## 六、改进亮点回顾

自 2026-04-22 以来，后端团队高质量修复了：
1. ✅ RBAC 路由级全量强制（14 处端点）
2. ✅ conn_manager TOCTOU
3. ✅ msg_service dedup TOCTOU（in_flight 机制）
4. ✅ friend_service 用户枚举差异
5. ✅ 前端 auth store 回滚
6. ✅ 前端各 view try/finally + loading 统一化
7. ✅ WebView2 cleanup 顺序
8. ✅ Win32UIDispatcher HWND 原子化
9. ✅ JsBridge lifecycle weak_ptr 防护
10. ✅ msg_service 广播预编码复用

仍需集中攻克的是**移动端 3 个崩溃点**与**客户端 SDK 的 2 个 UAF/幽灵回调风险** —— 这是本次复审的最高优先级。

---

## 七、后续行动

1. **本周内完成 P0-1..P0-5** —— 全部为崩溃/UAF，影响生产
2. **下周完成 P1-1..P1-5** —— 状态一致性与稳态
3. **引入 CI 增强**：TSAN、Android `CheckJNI`、iOS ASan、cppcheck、npm audit
4. **压测**：k6 模拟 1k 并发登录 + 10k 在线消息收发，观察 AuthMiddleware RBAC 查询成为瓶颈与否
5. **文档**：补权限矩阵文档；在 `docs/` 新增《移动端内存管理规约》
