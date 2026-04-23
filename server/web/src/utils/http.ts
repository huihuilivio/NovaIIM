import axios from 'axios'
import type { AxiosInstance, InternalAxiosRequestConfig, AxiosResponse } from 'axios'
import { getToken, removeToken } from './token'
import { ElMessage } from 'element-plus'
import router from '@/router'

// 统一响应格式
export interface ApiResponse<T = unknown> {
  code: number
  msg: string
  data: T
}

// 分页响应
export interface PaginatedData<T> {
  items: T[]
  total: number
  page: number
  page_size: number
  total_pages?: number
}

const http: AxiosInstance = axios.create({
  baseURL: '/api/v1',
  timeout: 15000,
})

// 请求拦截：注入 JWT
http.interceptors.request.use(
  (config: InternalAxiosRequestConfig) => {
    const token = getToken()
    if (token && config.headers) {
      config.headers.Authorization = `Bearer ${token}`
    }
    return config
  },
  (error) => Promise.reject(error),
)

// 响应拦截：处理 401
http.interceptors.response.use(
  (response: AxiosResponse<ApiResponse>) => {
    return response
  },
  async (error) => {
    if (error.response?.status === 401) {
      // 使用动态 import 延迟获取 store，避免 pinia 初始化顺序 / 循环依赖问题
      try {
        const { useAuthStore } = await import('@/stores/auth')
        const authStore = useAuthStore()
        // 清理 Pinia 状态（token + adminInfo）并跳转到 /login
        authStore.doLogout()
      } catch {
        // 退路：至少保证 token 被清除
        removeToken()
        if (router.currentRoute.value.path !== '/login') {
          router.push('/login')
        }
      }
      ElMessage.error('登录已过期，请重新登录')
    } else if (error.response?.status === 403) {
      ElMessage.error('无权限执行此操作')
    } else if (error.response?.status === 429) {
      ElMessage.error('操作过于频繁，请稍后重试')
    } else {
      ElMessage.error(error.message || '网络异常')
    }
    return Promise.reject(error)
  },
)

export default http
