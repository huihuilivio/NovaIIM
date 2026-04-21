<template>
  <el-container class="main-layout">
    <!-- 左侧导航栏 -->
    <el-aside :width="isCollapsed ? '64px' : '220px'" class="sidebar">
      <div class="logo" @click="router.push('/dashboard')">
        <el-icon :size="24"><Monitor /></el-icon>
        <span v-show="!isCollapsed" class="logo-text">NovaIIM</span>
      </div>
      <el-menu
        :default-active="route.path"
        :collapse="isCollapsed"
        :collapse-transition="false"
        background-color="#1d1e1f"
        text-color="#bfcbd9"
        active-text-color="#409eff"
        router
      >
        <el-menu-item
          v-for="item in menuItems"
          :key="item.path"
          :index="item.path"
        >
          <el-icon><component :is="item.icon" /></el-icon>
          <template #title>{{ item.title }}</template>
        </el-menu-item>
      </el-menu>
    </el-aside>

    <!-- 右侧工作区 -->
    <el-container class="main-container">
      <!-- 顶栏 -->
      <el-header class="header">
        <div class="header-left">
          <el-icon class="collapse-btn" @click="isCollapsed = !isCollapsed">
            <Fold v-if="!isCollapsed" />
            <Expand v-else />
          </el-icon>
          <el-breadcrumb separator="/">
            <el-breadcrumb-item :to="{ path: '/dashboard' }">首页</el-breadcrumb-item>
            <el-breadcrumb-item v-if="currentTitle">{{ currentTitle }}</el-breadcrumb-item>
          </el-breadcrumb>
        </div>
        <div class="header-right">
          <el-dropdown @command="handleCommand">
            <span class="user-info">
              <el-icon><UserFilled /></el-icon>
              {{ authStore.adminInfo?.nickname ?? authStore.adminInfo?.uid ?? '管理员' }}
              <el-icon class="el-icon--right"><ArrowDown /></el-icon>
            </span>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item command="logout">退出登录</el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
        </div>
      </el-header>

      <!-- 内容区 -->
      <el-main class="content">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const route = useRoute()
const router = useRouter()
const authStore = useAuthStore()
const isCollapsed = ref(false)

const menuItems = [
  { path: '/dashboard', title: '服务看板', icon: 'Monitor' },
  { path: '/users', title: '用户管理', icon: 'User' },
  { path: '/admins', title: '运维人员', icon: 'UserFilled' },
  { path: '/roles', title: '权限管理', icon: 'Lock' },
  { path: '/messages', title: '消息管理', icon: 'ChatDotRound' },
  { path: '/audit', title: '审计日志', icon: 'Document' },
]

const currentTitle = computed(() => {
  const matched = menuItems.find((m) => route.path.startsWith(m.path))
  return matched?.title
})

function handleCommand(cmd: string) {
  if (cmd === 'logout') {
    authStore.logout()
  }
}

onMounted(() => {
  if (authStore.isLoggedIn && !authStore.adminInfo) {
    authStore.fetchMe()
  }
})
</script>

<style scoped lang="scss">
.main-layout {
  height: 100vh;
}

.sidebar {
  background: var(--nova-sidebar-bg);
  transition: width 0.2s;
  overflow: hidden;

  .logo {
    height: var(--nova-header-height);
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    color: #fff;
    font-size: 18px;
    font-weight: 600;
    cursor: pointer;
    border-bottom: 1px solid #2c2d2e;
  }

  .logo-text {
    white-space: nowrap;
  }

  .el-menu {
    border-right: none;
  }
}

.main-container {
  overflow: hidden;
}

.header {
  height: var(--nova-header-height);
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: #fff;
  border-bottom: 1px solid var(--nova-border);
  padding: 0 20px;

  .header-left {
    display: flex;
    align-items: center;
    gap: 16px;
  }

  .collapse-btn {
    font-size: 20px;
    cursor: pointer;
    color: #606266;
    &:hover {
      color: #409eff;
    }
  }

  .header-right {
    .user-info {
      display: flex;
      align-items: center;
      gap: 6px;
      cursor: pointer;
      color: #606266;
      font-size: 14px;
    }
  }
}

.content {
  background: var(--nova-bg);
  overflow-y: auto;
}
</style>
