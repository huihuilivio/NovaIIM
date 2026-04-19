# NovaIIM Admin Web

后台管理面板 — Vue 3 + TypeScript + Element Plus

---

## 快速开始

### 前置条件

- **Node.js ≥ 18**（推荐 LTS 版本）
- **npm ≥ 9**
- 后端服务运行在 `localhost:9091`（Vite 自动代理 `/api`）

### 安装与启动

```bash
# 安装依赖
npm install

# 启动开发服务器（端口 3000，自动代理后端）
npm run dev

# 运行测试
npm run test

# 类型检查
npx vue-tsc --noEmit

# 生产构建
npm run build

# 预览生产构建
npm run preview
```

或使用统一脚本（从项目根目录）：

```bash
python scripts/admin_web.py dev       # 开发服务器
python scripts/admin_web.py build     # 类型检查 + 构建
python scripts/admin_web.py test      # 运行测试
```

---

## 项目结构

```
src/
├── api/         # REST 接口（auth, dashboard, user, message, audit, admin）
├── layout/      # MainLayout 侧边栏 + 顶栏
├── router/      # Vue Router 配置 + 导航守卫
├── stores/      # Pinia 状态管理（auth store）
├── styles/      # 全局 SCSS
├── utils/       # Axios 封装 + Token 工具
└── views/       # 7 个页面视图
```

---

## 开发代理

Vite 开发服务器将 `/api` 请求代理到后端：

```ts
// vite.config.ts
server: {
  port: 3000,
  proxy: {
    '/api': {
      target: 'http://127.0.0.1:9091',
      changeOrigin: true,
    },
  },
}
```

确保后端 NovaIIM 服务已启动（Admin HTTP 端口 9091），登录页才能正常工作。

**默认管理员账户：**
- 账号：`admin`
- 密码：`admin123`（首次运行自动创建 super admin）

---

## 测试

```bash
npm run test              # 单次运行
npm run test:watch        # 监听模式
npm run test:coverage     # 覆盖率报告
```

**测试环境：** Vitest + happy-dom  
**当前覆盖：** 8 个用例（token 工具 / auth 状态 / 路由 / 登录页渲染）

---

## 生产构建

```bash
npm run build
```

输出目录：`dist/`  
部署时使用 Nginx 提供静态文件服务，并配置 `/api` 反向代理：

```nginx
server {
    listen 80;
    root /path/to/admin-web/dist;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api {
        proxy_pass http://127.0.0.1:9091;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

---

## 技术栈

| 分类 | 技术 | 版本 |
|------|------|------|
| 框架 | Vue 3 | 3.5 |
| 语言 | TypeScript | 6.0 |
| 构建 | Vite | 8.x |
| UI 库 | Element Plus | 2.13 |
| 状态 | Pinia | 3.0 |
| 路由 | Vue Router | 4.6 |
| HTTP | Axios | 1.9 |
| 测试 | Vitest | 3.x |
| 样式 | SCSS | - |
