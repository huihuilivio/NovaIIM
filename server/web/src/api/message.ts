import http from '@/utils/http'
import type { ApiResponse, PaginatedData } from '@/utils/http'

export interface Message {
  id: number
  conversation_id: number
  sender_uid: string
  content: string
  status: number
  seq: number
  created_at: string
}

export interface MessageQuery {
  page?: number
  page_size?: number
  conversation_id?: number
  start_time?: string
  end_time?: string
}

export function getMessages(params: MessageQuery) {
  return http.get<ApiResponse<PaginatedData<Message>>>('/messages', { params })
}

export function recallMessage(id: number, reason: string) {
  return http.post<ApiResponse<object>>(`/messages/${id}/recall`, { reason })
}
