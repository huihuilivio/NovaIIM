import { defineStore } from 'pinia'
import { ref } from 'vue'
import { bridge } from '@/bridge'
import { useConnectionStore } from './connection'
import router from '@/router'

export interface LoginResult {
  success: boolean
  uid?: string
  nickname?: string
  avatar?: string
  msg?: string
}

export interface RegisterResult {
  success: boolean
  uid?: string
  msg?: string
}

export const useAuthStore = defineStore('auth', () => {
  const uid = ref('')
  const nickname = ref('')
  const isLoggedIn = ref(false)

  function login(email: string, password: string): Promise<LoginResult> {
    const conn = useConnectionStore()
    conn.connect()

    return new Promise((resolve) => {
      const timeout = setTimeout(() => {
        bridge.off('loginResult')
        resolve({ success: false, msg: '连接超时，请重试' })
      }, 15000)

      bridge.send('login', { email, password })

      bridge.on<LoginResult>('loginResult', (data) => {
        clearTimeout(timeout)
        bridge.off('loginResult')
        if (data.success) {
          uid.value = data.uid!
          nickname.value = data.nickname || data.uid!
          isLoggedIn.value = true
        }
        resolve(data)
      })
    })
  }

  function register(
    email: string,
    nick: string,
    password: string,
  ): Promise<RegisterResult> {
    const conn = useConnectionStore()
    conn.connect()

    return new Promise((resolve) => {
      const timeout = setTimeout(() => {
        bridge.off('registerResult')
        resolve({ success: false, msg: '连接超时，请重试' })
      }, 15000)

      bridge.send('register', { email, nickname: nick, password })

      bridge.on<RegisterResult>('registerResult', (data) => {
        clearTimeout(timeout)
        bridge.off('registerResult')
        resolve(data)
      })
    })
  }

  function logout() {
    const conn = useConnectionStore()
    conn.disconnect()
    uid.value = ''
    nickname.value = ''
    isLoggedIn.value = false
  }

  // 监听被踢下线事件
  bridge.on<{ reason: number; msg: string }>('kicked', (data) => {
    uid.value = ''
    nickname.value = ''
    isLoggedIn.value = false
    router.push({ name: 'login', query: { kicked: data.msg || 'You have been kicked offline' } })
  })

  return { uid, nickname, isLoggedIn, login, register, logout }
})
