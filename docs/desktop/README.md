# NovaIIM Desktop Client

> Win32 + WebView2 桌面客户端

---

## 概览

NovaIIM 桌面客户端使用 **Win32 窗口 + Microsoft WebView2** 架构，C++ 后端通过 `nova_sdk` 处理 IM 逻辑，前端使用纯 HTML/CSS/JS 实现 UI。

```
┌──────────────────────────────────────────┐
│  Win32 Window (HWND)                     │
│  ┌────────────────────────────────────┐  │
│  │  WebView2 (Chromium)               │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │  HTML / CSS / JS             │  │  │
│  │  │  (login.js / main.js)        │  │  │
│  │  └──────────┬───────────────────┘  │  │
│  │             │ chrome.webview       │  │
│  │             │ postMessage          │  │
│  └─────────────┼──────────────────────┘  │
│                │                         │
│  ┌─────────────▼──────────────────────┐  │
│  │  JsBridge (C++)                    │  │
│  │  ↔ NovaBridge (JS)                │  │
│  └─────────────┬──────────────────────┘  │
│                │                         │
│  ┌─────────────▼──────────────────────┐  │
│  │  nova_sdk (NovaClient / VMs)       │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

---

## 文件结构

```
client/desktop/
├── CMakeLists.txt          # 平台路由（当前仅 Windows）
├── web/                    # 前端资源
│   ├── index.html          # SPA 入口
│   ├── css/style.css       # 主题样式
│   └── js/
│       ├── bridge.js       # NovaBridge — JS ↔ C++ 通信层
│       ├── login.js        # 登录/注册页面
│       ├── main.js         # 主界面（三栏布局）
│       └── app.js          # SPA 路由器
└── win/                    # Windows 平台实现
    ├── CMakeLists.txt      # 构建配置 + WebView2 SDK 下载
    ├── main.cpp            # wWinMain 入口
    ├── webview2_app.h/cpp  # Win32 窗口 + WebView2 生命周期
    ├── win32_ui_dispatcher.h/cpp  # PostMessage UI 线程投递
    ├── js_bridge.h/cpp     # C++ ↔ JS 双向通信桥
    ├── app.rc              # Windows 资源文件（图标）
    └── app.ico             # 应用图标（蓝色 N）
```

---

## C++ ↔ JS 通信协议

### JS → C++ (Actions)

通过 `NovaBridge.send(action, data)` 发送：

| Action | 参数 | 说明 |
|--------|------|------|
| `connect` | — | 连接服务器 |
| `disconnect` | — | 断开连接 |
| `login` | `{email, password}` | 登录 |
| `register` | `{email, nickname, password}` | 注册 |
| `sendMessage` | `{to, content}` | 发送消息 |

### C++ → JS (Events)

通过 `NovaBridge.on(event, callback)` 订阅：

| Event | 数据 | 说明 |
|-------|------|------|
| `loginResult` | `{success, uid, nickname, msg}` | 登录结果 |
| `registerResult` | `{success, uid, msg}` | 注册结果 |
| `connectionState` | `{state}` | 连接状态变更 |
| `newMessage` | `{conversationId, senderUid, content, serverSeq, serverTime, msgType}` | 新消息 |
| `sendMsgResult` | `{success, serverSeq, serverTime, msg}` | 发送结果 |
| `recallNotify` | `{conversationId, serverSeq, operatorUid}` | 撤回通知 |

### 通信实现

**JS 端 (bridge.js)**：
```javascript
NovaBridge.send('login', { email: 'user@example.com', password: '123456' });
NovaBridge.on('loginResult', function(data) { /* ... */ });
```

**C++ 端 (js_bridge.cpp)**：
- `OnWebMessage()` → 解析 JSON → 分发到 `Handle*()` 方法
- `PostEvent()` → 构造 JS 调用 → 通过 `UIDispatcher::Post()` 回到 UI 线程 → `ExecuteScript()`

---

## UI 页面

### 登录/注册页 (login.js)

- 两个表单可切换：登录 ↔ 注册
- **登录**：邮箱 + 密码，15 秒超时
- **注册**：邮箱 + 昵称 + 密码 + 确认密码
  - 前端校验：密码一致性、最小 6 位
  - 注册成功自动切回登录，回填邮箱
- 按钮防重复提交（disabled + 文案切换）

### 主界面 (main.js)

三栏布局：

```
┌────────┬──────────────┬─────────────────────────┐
│ 侧边栏  │  会话列表     │  聊天区                  │
│ (64px) │  (280px)     │  (flex: 1)              │
│        │              │                          │
│ [头像]  │ [搜索框]      │ [聊天头部]                │
│ [💬]   │ [会话1]       │ [消息气泡...]             │
│ [👤]   │ [会话2]       │                          │
│ [👥]   │ [会话3]       │ [输入框] [发送]           │
│        │              │                          │
│ [⚙]   │              │                          │
├────────┴──────────────┴─────────────────────────┤
│ 状态栏 (24px) — ● 已连接                         │
└─────────────────────────────────────────────────┘
```

- 连接状态实时显示（绿/黄/红点）
- Enter 键发送消息
- XSS 防护（`escapeHtml()`）

---

## 生命周期

```
wWinMain()
  ├── CoInitializeEx(COINIT_APARTMENTTHREADED)
  ├── NovaClient(config_path)
  ├── client.Init()
  ├── WebView2App(hInstance, &client)
  ├── app.Init(nCmdShow)
  │     ├── CreateWindowExW()
  │     ├── Win32UIDispatcher::SetHwnd() + Install()
  │     └── InitWebView2()  (异步)
  │           └── OnWebViewReady()
  │                 ├── JsBridge(webview, &client)
  │                 ├── bridge.Init()  → 缓存 VM + 订阅事件
  │                 ├── SetVirtualHostNameToFolderMapping("novaim.local")
  │                 └── Navigate("https://novaim.local/index.html")
  ├── app.Run()  (Win32 消息循环)
  │     └── WM_DESTROY → bridge_.reset() → PostQuitMessage()
  ├── client.Shutdown()
  └── CoUninitialize()
```

### 线程安全

- **网络回调 → UI 线程**：所有 `PostEvent()` 通过 `UIDispatcher::Post()` 回到 UI 线程
- **生命周期守护**：`PostEvent` lambda 捕获 `weak_ptr<atomic<bool>> alive_`，执行前检查 JsBridge 是否已销毁
- **关闭顺序**：`WM_DESTROY` 先销毁 `JsBridge`，再 `PostQuitMessage`，确保 bridge 在 `client.Shutdown()` 之前释放

---

## 构建

```bash
cmake -B build -DNOVA_BUILD_CLIENT=ON
cmake --build build --target nova_desktop

# WebView2 SDK 自动下载 (NuGet)
# 输出: output/bin/nova_desktop.exe + output/bin/web/
```

### 依赖

- **WebView2 Runtime** — Microsoft Edge WebView2（自动下载 NuGet SDK）
- **nova_sdk.dll** — 自动复制到输出目录
- **Windows 10 1809+** — WebView2 最低系统要求
