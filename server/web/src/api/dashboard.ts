import http from '@/utils/http'
import type { ApiResponse } from '@/utils/http'

export interface DashboardStats {
  connections: number
  online_users: number
  messages_today: number
  bad_packets: number
  uptime_seconds: number
  cpu_percent: number
  memory_mb: number
  timestamp: string
}

export function getDashboardStats() {
  return http.get<ApiResponse<DashboardStats>>('/dashboard/stats')
}
