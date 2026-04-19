import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'
import { getToken } from '@/utils/token'

const Layout = () => import('@/layout/MainLayout.vue')

const routes: RouteRecordRaw[] = [
  {
    path: '/login',
    name: 'Login',
    component: () => import('@/views/login/LoginView.vue'),
    meta: { title: '登录', public: true },
  },
  {
    path: '/',
    component: Layout,
    redirect: '/dashboard',
    children: [
      {
        path: 'dashboard',
        name: 'Dashboard',
        component: () => import('@/views/dashboard/DashboardView.vue'),
        meta: { title: '服务看板', icon: 'Monitor' },
      },
      {
        path: 'users',
        name: 'Users',
        component: () => import('@/views/users/UserList.vue'),
        meta: { title: '用户管理', icon: 'User' },
      },
      {
        path: 'admins',
        name: 'Admins',
        component: () => import('@/views/admins/AdminList.vue'),
        meta: { title: '运维人员', icon: 'UserFilled' },
      },
      {
        path: 'roles',
        name: 'Roles',
        component: () => import('@/views/roles/RoleList.vue'),
        meta: { title: '权限管理', icon: 'Lock' },
      },
      {
        path: 'messages',
        name: 'Messages',
        component: () => import('@/views/messages/MessageList.vue'),
        meta: { title: '消息管理', icon: 'ChatDotRound' },
      },
      {
        path: 'audit',
        name: 'Audit',
        component: () => import('@/views/audit/AuditList.vue'),
        meta: { title: '审计日志', icon: 'Document' },
      },
    ],
  },
  {
    path: '/:pathMatch(.*)*',
    redirect: '/dashboard',
  },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

// 导航守卫
router.beforeEach((to, _from, next) => {
  const token = getToken()
  if (to.meta.public) {
    // 已登录访问登录页 → 跳转首页
    if (token && to.path === '/login') {
      next('/dashboard')
    } else {
      next()
    }
  } else {
    // 需要认证
    if (!token) {
      next('/login')
    } else {
      next()
    }
  }
})

router.afterEach((to) => {
  document.title = `${(to.meta as { title?: string }).title ?? 'NovaIIM'} - NovaIIM Admin`
})

export default router
