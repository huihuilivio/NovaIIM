import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useAuthStore } from '@/stores/auth'
import { removeToken } from '@/utils/token'

describe('Auth Store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    removeToken()
  })

  it('初始状态未登录', () => {
    const store = useAuthStore()
    expect(store.isLoggedIn).toBe(false)
    expect(store.adminInfo).toBeNull()
    expect(store.permissions).toEqual([])
  })

  it('hasPermission 未登录返回 false', () => {
    const store = useAuthStore()
    expect(store.hasPermission('admin.dashboard')).toBe(false)
  })
})
