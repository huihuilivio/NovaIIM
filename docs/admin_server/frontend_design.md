# Admin Web 前端设计文档

> **技术栈：** Vue 3 + TypeScript + Vite + Element Plus + Pinia  
> **状态：** ✅ M1 + M4 框架完成 | 8 测试通过 | 最后更新：2026-04-19

---

## 1. 架构概览

```
admin-web/
├── src/
│   ├── api/              # API 接口层 — 与后端 REST 端点一一对应
│   │   ├── auth.ts       # POST /auth/login, /logout, GET /auth/me
│   │   ├── dashboard.ts  # GET /dashboard/stats
│   │   ├── user.ts       # GET/POST/DELETE /users, ban/unban/kick/resetPwd
│   │   ├── message.ts    # GET /messages, POST /messages/:id/recall
│   │   ├── audit.ts      # GET /audit-logs
│   │   └── admin.ts      # 管理员 CRUD + 角色 CRUD + 权限列表
│   │
│   ├── layout/           # 页面布局
│   │   └── MainLayout.vue  # 左侧导航栏 + 顶栏 + 内容区
│   │
│   ├── router/           # 路由配置
│   │   └── index.ts      # 路由表 + beforeEach 守卫（未登录 → /login）
│   │
│   ├── stores/           # Pinia 状态管理
│   │   └── auth.ts       # token / adminInfo / permissions / login / logout
│   │
│   ├── utils/            # 工具函数
│   │   ├── http.ts       # Axios 实例（JWT 拦截、401/403/429 处理）
│   │   └── token.ts      # localStorage token 存取
│   │
│   ├── views/            # 页面视图
│   │   ├── login/        # 登录页
│   │   ├── dashboard/    # 服务看板
│   │   ├── users/        # 用户管理
│   │   ├── admins/       # 运维人员管理
│   │   ├── roles/        # 权限管理（角色 + 权限分配）
│   │   ├── messages/     # 消息管理
│   │   └── audit/        # 审计日志
│   │
│   ├── styles/           # 全局样式
│   │   └── index.scss    # CSS 变量、Reset、滚动条
│   │
│   ├── App.vue           # 根组件（仅 router-view）
│   └── main.ts           # 入口（Pinia + Router + ElementPlus）
│
├── vite.config.ts        # Vite 配置（proxy /api → :9091）
├── vitest.config.ts      # 测试配置（happy-dom 环境）
└── tsconfig.json         # TypeScript 配置
```

---

## 2. 数据流

```
用户操作 → Vue 组件
             ↓ dispatch
         Pinia Store (auth / 业务状态)
             ↓ call
         API 层 (src/api/*.ts)
             ↓ axios
         HTTP 请求 → Vite Proxy → 后端 :9091
             ↓
         统一响应 { code, msg, data }
             ↓
         拦截器处理 (401 → 清 token → /login)
             ↓
         组件更新 UI
```

---

## 3. 认证流程

```
1. 用户输入账号密码 → LoginView.vue
2. authStore.login({ uid, password })
3. POST /api/v1/auth/login → { token, admin_id }
4. setToken(token) → localStorage
5. authStore.fetchMe() → GET /api/v1/auth/me
6. 存储 adminInfo（uid, nickname, roles, permissions）
7. router.push('/dashboard')

每次请求：
- Axios 请求拦截器 → headers.Authorization = `Bearer ${token}`
- 401 响应 → removeToken() → router.push('/login')
```

---

## 4. 路由与权限

| 路径 | 组件 | 说明 | 需登录 |
|------|------|------|--------|
| `/login` | LoginView | 登录页 | 否 |
| `/dashboard` | DashboardView | 服务看板 | 是 |
| `/users` | UserList | 用户管理 | 是 |
| `/admins` | AdminList | 运维人员 | 是 |
| `/roles` | RoleList | 权限管理 | 是 |
| `/messages` | MessageList | 消息管理 | 是 |
| `/audit` | AuditList | 审计日志 | 是 |

**导航守卫逻辑：**
- 无 token + 非 public 路由 → 重定向 `/login`
- 有 token + 访问 `/login` → 重定向 `/dashboard`
- 路由切换后更新 `document.title`

---

## 5. UI 设计规范

- **风格：** Flat 扁平风格，深色侧边栏 (#1d1e1f) + 浅色工作区 (#f5f7fa)
- **侧边栏：** 220px，可折叠至 64px（图标模式）
- **顶栏：** 56px，面包屑 + 用户下拉菜单
- **组件库：** Element Plus 全量引入 + 图标全局注册
- **字体：** Helvetica Neue / PingFang SC / Microsoft YaHei

---

## 6. 测试策略

| 层级 | 工具 | 覆盖范围 |
|------|------|----------|
| 单元测试 | Vitest + happy-dom | utils, stores, router 配置 |
| 组件测试 | @vue/test-utils | 关键页面渲染验证 |
| 类型检查 | vue-tsc | 全量 TypeScript + Vue SFC |

**当前测试 (8 个用例)：**
- `token.test.ts` — setToken/getToken/removeToken (3)
- `auth.test.ts` — 初始状态 / hasPermission (2)
- `router.test.ts` — 路由表完整性 (2)
- `login.test.ts` — 登录页渲染 (1)

---

## 7. 开发命令

```bash
# 从项目根目录执行
python scripts/admin_web.py install     # 安装依赖
python scripts/admin_web.py dev         # 启动开发服务器 (localhost:3000)
python scripts/admin_web.py build       # 类型检查 + 生产构建
python scripts/admin_web.py test        # 运行单元测试
python scripts/admin_web.py typecheck   # 仅类型检查
python scripts/admin_web.py preview     # 预览生产构建

# 或直接使用 npm（在 admin-web/ 目录下）
npm run dev          # 开发服务器
npm run build        # vue-tsc + vite build
npm run test         # vitest run
npm run test:watch   # vitest 监听模式
npm run test:coverage  # 带覆盖率
```

---

## 8. 与后端的对接

- **开发环境：** Vite proxy `/api` → `http://127.0.0.1:9091` (后端 Admin HTTP)
- **生产部署：** Nginx 反向代理，静态文件 `admin-web/dist/` + API 转发
- **API 格式：** 统一 `{ code: 0, msg: "success", data: {...} }`
- **分页约定：** `?page=1&page_size=20`，响应 `{ items, total, page, page_size }`
