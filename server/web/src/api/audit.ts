import http from '@/utils/http'
import type { ApiResponse, PaginatedData } from '@/utils/http'

export interface AuditLog {
  id: number
  admin_id: number
  operator_uid: string
  action: string
  target_type: string
  target_id: number
  detail: string
  ip: string
  created_at: string
}

export interface AuditQuery {
  page?: number
  page_size?: number
  admin_id?: number
  action?: string
  start_time?: string
  end_time?: string
}

export function getAuditLogs(params: AuditQuery) {
  return http.get<ApiResponse<PaginatedData<AuditLog>>>('/audit-logs', { params })
}
