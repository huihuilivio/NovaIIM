import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { getToken, setToken, removeToken } from '@/utils/token'
import { login as loginApi, logout as logoutApi, getMe } from '@/api/auth'
import type { AdminInfo, LoginReq } from '@/api/auth'
import router from '@/router'

export const useAuthStore = defineStore('auth', () => {
  const token = ref<string | null>(getToken())
  const adminInfo = ref<AdminInfo | null>(null)

  const isLoggedIn = computed(() => !!token.value)
  const permissions = computed(() => adminInfo.value?.permissions ?? [])

  function hasPermission(perm: string): boolean {
    return permissions.value.includes(perm)
  }

  async function login(data: LoginReq) {
    const res = await loginApi(data)
    const d = res.data
    if (d.code !== 0) throw new Error(d.msg)
    token.value = d.data.token
    setToken(d.data.token)
    await fetchMe()
  }

  async function fetchMe() {
    try {
      const res = await getMe()
      if (res.data.code === 0) {
        adminInfo.value = res.data.data
      }
    } catch {
      // token 无效
      doLogout()
    }
  }

  async function logout() {
    try {
      await logoutApi()
    } finally {
      doLogout()
    }
  }

  function doLogout() {
    token.value = null
    adminInfo.value = null
    removeToken()
    router.push('/login')
  }

  return { token, adminInfo, isLoggedIn, permissions, hasPermission, login, fetchMe, logout }
})
