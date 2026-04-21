import { defineStore } from 'pinia'
import { ref } from 'vue'
import { bridge } from '@/bridge'

export interface Conversation {
  id: number
  name: string
  lastMsg: string
  time: string
}

export interface ChatMessage {
  sender: string
  content: string
  self: boolean
}

export interface ReceivedMessage {
  conversationId: number
  senderUid: string
  content: string
  serverSeq: number
  serverTime: number
  msgType: number
}

export const useChatStore = defineStore('chat', () => {
  const conversations = ref<Conversation[]>([
    { id: 1, name: '张三', lastMsg: '你好！', time: '10:30' },
    { id: 2, name: '项目群', lastMsg: '[图片]', time: '昨天' },
    { id: 3, name: '李四', lastMsg: '好的', time: '周一' },
  ])

  const activeConv = ref<Conversation | null>(null)
  const messages = ref<ChatMessage[]>([])

  function init() {
    bridge.on<ReceivedMessage>('newMessage', (data) => {
      const conv = conversations.value.find((c) => c.id === data.conversationId)
      if (conv) {
        conv.lastMsg = data.content
        conv.time = formatTime(data.serverTime)
      }
      if (activeConv.value?.id === data.conversationId) {
        messages.value.push({
          sender: data.senderUid,
          content: data.content,
          self: false,
        })
      }
    })

    bridge.on<{ success: boolean; msg?: string }>('sendMsgResult', (data) => {
      if (!data.success) {
        console.warn('Send failed:', data.msg)
      }
    })
  }

  function selectConversation(conv: Conversation) {
    activeConv.value = conv
    messages.value = []
  }

  function sendMessage(content: string, senderUid: string) {
    if (!activeConv.value) return

    bridge.send('sendMessage', {
      to: String(activeConv.value.id),
      content,
    })

    messages.value.push({
      sender: senderUid,
      content,
      self: true,
    })
  }

  function formatTime(epochMs: number): string {
    if (!epochMs) return ''
    const d = new Date(epochMs)
    return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`
  }

  return { conversations, activeConv, messages, init, selectConversation, sendMessage }
})
