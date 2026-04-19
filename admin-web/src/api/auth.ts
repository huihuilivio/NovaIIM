import http from '@/utils/http'
import type { ApiResponse } from '@/utils/http'

export interface LoginReq {
  uid: string
  password: string
}

export interface LoginData {
  admin_id: number
  token: string
  token_type: string
  expires_in: number
}

export interface AdminInfo {
  admin_id: number
  uid: string
  nickname: string
  status: number
  roles: string[]
  permissions: string[]
}

export function login(data: LoginReq) {
  return http.post<ApiResponse<LoginData>>('/auth/login', data)
}

export function logout() {
  return http.post<ApiResponse<object>>('/auth/logout')
}

export function getMe() {
  return http.get<ApiResponse<AdminInfo>>('/auth/me')
}
