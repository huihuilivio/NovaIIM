import http from '@/utils/http'
import type { ApiResponse, PaginatedData } from '@/utils/http'

// ── 管理员 ──
export interface Admin {
  id: number
  uid: string
  nickname: string
  status: number
  created_at: string
  updated_at: string
  roles: string[]
}

export interface AdminQuery {
  page?: number
  page_size?: number
  keyword?: string
}

export function getAdmins(params: AdminQuery) {
  return http.get<ApiResponse<PaginatedData<Admin>>>('/admins', { params })
}

export function createAdmin(data: { uid: string; password: string; nickname?: string }) {
  return http.post<ApiResponse<{ id: number }>>('/admins', data)
}

export function deleteAdmin(id: number) {
  return http.delete<ApiResponse<object>>(`/admins/${id}`)
}

// ── 角色 ──
export interface Role {
  id: number
  name: string
  description: string
  permissions: string[]
  created_at: string
}

export function getRoles() {
  return http.get<ApiResponse<Role[]>>('/roles')
}

export function createRole(data: { name: string; description?: string; permissions: string[] }) {
  return http.post<ApiResponse<{ id: number }>>('/roles', data)
}

export function updateRole(id: number, data: { description?: string; permissions: string[] }) {
  return http.put<ApiResponse<object>>(`/roles/${id}`, data)
}

export function deleteRole(id: number) {
  return http.delete<ApiResponse<object>>(`/roles/${id}`)
}

// ── 权限列表 ──
export const ALL_PERMISSIONS = [
  'admin.login',
  'admin.dashboard',
  'admin.audit',
  'admin.manage',
  'user.view',
  'user.create',
  'user.edit',
  'user.delete',
  'user.ban',
  'msg.view',
  'msg.recall',
] as const
