import { describe, it, expect } from 'vitest'
import { getToken, setToken, removeToken } from '@/utils/token'

describe('Token 管理', () => {
  it('setToken / getToken 应正确存取', () => {
    setToken('test-jwt-token')
    expect(getToken()).toBe('test-jwt-token')
  })

  it('removeToken 应清除 token', () => {
    setToken('test-jwt-token')
    removeToken()
    expect(getToken()).toBeNull()
  })

  it('getToken 在无 token 时返回 null', () => {
    removeToken()
    expect(getToken()).toBeNull()
  })
})
