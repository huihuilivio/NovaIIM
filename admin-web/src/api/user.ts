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

export function getUserDetail(id: number) {
  return http.get<ApiResponse<UserDetail>>(`/users/${id}`)
}

export function createUser(data: CreateUserReq) {
  return http.post<ApiResponse<{ id: number; email: string }>>('/users', data)
}

export function deleteUser(id: number) {
  return http.delete<ApiResponse<object>>(`/users/${id}`)
}

export function resetPassword(id: number, new_password: string) {
  return http.post<ApiResponse<object>>(`/users/${id}/reset-password`, { new_password })
}

export function banUser(id: number, reason: string) {
  return http.post<ApiResponse<object>>(`/users/${id}/ban`, { reason })
}

export function unbanUser(id: number) {
  return http.post<ApiResponse<object>>(`/users/${id}/unban`)
}

export function kickUser(id: number) {
  return http.post<ApiResponse<{ kicked_devices: number }>>(`/users/${id}/kick`)
}
