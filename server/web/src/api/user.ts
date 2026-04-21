import http from '@/utils/http'
import type { ApiResponse, PaginatedData } from '@/utils/http'

export interface User {
  id: number
  uid: string
  nickname: string
  status: number
  is_online: boolean
  device_count: number
  created_at: string
  updated_at: string
}

export interface UserDetail extends User {
  devices: { device_id: number; device_name: string; is_online: boolean; last_seen: string }[]
}

export interface UserQuery {
  page?: number
  page_size?: number
  keyword?: string
  status?: number
}

export interface CreateUserReq {
  email: string
  password: string
  nickname?: string
}

export function getUsers(params: UserQuery) {
  return http.get<ApiResponse<PaginatedData<User>>>('/users', { params })
}

export function getUserDetail(uid: string) {
  return http.get<ApiResponse<UserDetail>>(`/users/${uid}`)
}

export function createUser(data: CreateUserReq) {
  return http.post<ApiResponse<{ id: number; email: string }>>('/users', data)
}

export function deleteUser(uid: string) {
  return http.delete<ApiResponse<object>>(`/users/${uid}`)
}

export function resetPassword(uid: string, new_password: string) {
  return http.post<ApiResponse<object>>(`/users/${uid}/reset-password`, { new_password })
}

export function banUser(uid: string, reason: string) {
  return http.post<ApiResponse<object>>(`/users/${uid}/ban`, { reason })
}

export function unbanUser(uid: string) {
  return http.post<ApiResponse<object>>(`/users/${uid}/unban`)
}

export function kickUser(uid: string) {
  return http.post<ApiResponse<{ kicked_devices: number }>>(`/users/${uid}/kick`)
}
