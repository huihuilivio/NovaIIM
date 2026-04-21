import { defineStore } from 'pinia'
import { ref } from 'vue'
import { bridge } from '@/bridge'

// ---- 通知声音 (Web Audio API，无需外部音频文件) ----
let audioCtx: AudioContext | null = null

function playNotificationSound() {
  try {
    if (localStorage.getItem('nova_sound') === 'false') return
    if (localStorage.getItem('nova_notify') === 'false') return
  } catch { /* ignore */ }

  try {
    if (!audioCtx) audioCtx = new AudioContext()
    const osc = audioCtx.createOscillator()
    const gain = audioCtx.createGain()
    osc.connect(gain)
    gain.connect(audioCtx.destination)
    osc.type = 'sine'
    osc.frequency.setValueAtTime(880, audioCtx.currentTime)     // A5
    osc.frequency.setValueAtTime(1047, audioCtx.currentTime + 0.1) // C6
    gain.gain.setValueAtTime(0.3, audioCtx.currentTime)
    gain.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.3)
    osc.start(audioCtx.currentTime)
    osc.stop(audioCtx.currentTime + 0.3)
  } catch { /* AudioContext not available */ }
}

export interface LastMsg {
  senderUid: string
  senderNickname: string
  content: string
  msgType: number
  serverTime: number
}

export interface Conversation {
  conversationId: number
  type: number // 1=私聊, 2=群聊
  name: string
  avatar: string
  unreadCount: number
  lastMsg: LastMsg
  mute: number
  pinned: number
  updatedAt: string
}

export interface ChatMessage {
  sender: string
  content: string
  self: boolean
  serverSeq: number
  serverTime: number
  msgType: number
  status: number // 0=normal, 1=recalled
  sendStatus?: 'sending' | 'sent' | 'failed' // 客户端发送状态
  localId?: number // 本地消息ID用于匹配sendResult
}

export interface ReceivedMessage {
  conversationId: number
  senderUid: string
  content: string
  serverSeq: number
  serverTime: number
  msgType: number
}

export interface SyncMessage {
  serverSeq: number
  senderUid: string
  content: string
  msgType: number
  serverTime: string
  status: number
}

export interface RecallNotification {
  conversationId: number
  serverSeq: number
  operatorUid: string
}

export const useChatStore = defineStore('chat', () => {
  const conversations = ref<Conversation[]>([])
  const activeConv = ref<Conversation | null>(null)
  const messages = ref<ChatMessage[]>([])
  const loadingHistory = ref(false)
  const loadingConversations = ref(false)
  const hasMore = ref(false)
  const currentUid = ref('')
  let initialized = false
  let localIdCounter = 0

  function init(uid: string) {
    if (initialized) return
    initialized = true
    currentUid.value = uid

    // 会话列表从服务端获取
    bridge.on<{ success: boolean; conversations: Conversation[] }>('conversationList', (data) => {
      loadingConversations.value = false
      if (data.success) {
        conversations.value = data.conversations
        // 如果当前有选中的会话，更新引用
        if (activeConv.value) {
          const updated = data.conversations.find(
            (c) => c.conversationId === activeConv.value!.conversationId,
          )
          if (updated) activeConv.value = updated
        }
      }
    })

    // 收到新消息
    bridge.on<ReceivedMessage>('newMessage', (data) => {
      // 更新会话列表中的最后消息
      const conv = conversations.value.find((c) => c.conversationId === data.conversationId)
      if (conv) {
        conv.lastMsg = {
          senderUid: data.senderUid,
          senderNickname: '',
          content: data.content,
          msgType: data.msgType,
          serverTime: data.serverTime,
        }
        if (activeConv.value?.conversationId !== data.conversationId) {
          conv.unreadCount++
        }
        // 非自己发的消息 → 播放通知声音
        if (data.senderUid !== currentUid.value) {
          playNotificationSound()
        }
        // 将会话置顶排序
        const idx = conversations.value.indexOf(conv)
        if (idx > 0) {
          conversations.value.splice(idx, 1)
          conversations.value.unshift(conv)
        }
      }

      // 追加到当前聊天窗口
      if (activeConv.value?.conversationId === data.conversationId) {
        messages.value.push({
          sender: data.senderUid,
          content: data.content,
          self: data.senderUid === currentUid.value,
          serverSeq: data.serverSeq,
          serverTime: data.serverTime,
          msgType: data.msgType,
          status: 0,
        })
      }
    })

    // 发送结果
    bridge.on<{ success: boolean; msg?: string; serverSeq?: number; serverTime?: number }>(
      'sendMsgResult',
      (data) => {
        // 找到最近的 sending 状态消息并更新
        const pending = messages.value.find((m) => m.self && m.sendStatus === 'sending')
        if (pending) {
          if (data.success) {
            pending.sendStatus = 'sent'
            if (data.serverSeq) pending.serverSeq = data.serverSeq
            if (data.serverTime) pending.serverTime = data.serverTime
          } else {
            pending.sendStatus = 'failed'
          }
        }
      },
    )

    // 消息历史同步
    bridge.on<{ success: boolean; messages: SyncMessage[]; hasMore: boolean }>(
      'syncMessagesResult',
      (data) => {
        loadingHistory.value = false
        if (data.success) {
          hasMore.value = data.hasMore
          const older = data.messages.map((m) => ({
            sender: m.senderUid,
            content: m.content,
            self: m.senderUid === currentUid.value,
            serverSeq: m.serverSeq,
            serverTime: 0,
            msgType: m.msgType,
            status: m.status,
          }))
          messages.value = [...older, ...messages.value]
        }
      },
    )

    // 消息撤回
    bridge.on<RecallNotification>('recallNotify', (data) => {
      if (activeConv.value?.conversationId === data.conversationId) {
        const msg = messages.value.find((m) => m.serverSeq === data.serverSeq)
        if (msg) {
          msg.status = 1
          msg.content = '消息已撤回'
        }
      }
    })

    // 会话更新通知
    bridge.on('convUpdate', () => {
      loadConversations()
    })

    // 未读同步
    bridge.on<{ success: boolean; items: { conversationId: number; count: number }[] }>(
      'syncUnreadResult',
      (data) => {
        if (data.success) {
          for (const item of data.items) {
            const conv = conversations.value.find((c) => c.conversationId === item.conversationId)
            if (conv) conv.unreadCount = item.count
          }
        }
      },
    )
  }

  function loadConversations() {
    loadingConversations.value = true
    bridge.send('getConversationList')
  }

  function selectConversation(conv: Conversation) {
    activeConv.value = conv
    messages.value = []
    hasMore.value = false
    conv.unreadCount = 0

    // 加载历史消息
    loadingHistory.value = true
    bridge.send('syncMessages', {
      conversationId: conv.conversationId,
      lastSeq: 0,
      limit: 30,
    })

    // 发送已读
    bridge.send('sendReadAck', {
      conversationId: conv.conversationId,
      readUpToSeq: 0, // server will use latest
    })
  }

  function loadMoreHistory() {
    if (loadingHistory.value || !hasMore.value || !activeConv.value) return
    const oldestSeq = messages.value.length > 0 ? messages.value[0].serverSeq : 0
    loadingHistory.value = true
    bridge.send('syncMessages', {
      conversationId: activeConv.value.conversationId,
      lastSeq: oldestSeq > 0 ? oldestSeq - 1 : 0,
      limit: 30,
    })
  }

  function sendMessage(content: string, msgType: number = 0) {
    if (!activeConv.value) return
    const lid = ++localIdCounter

    bridge.send('sendMessage', {
      to: String(activeConv.value.conversationId),
      content,
    })

    // 乐观更新 — 立即显示自己发的消息
    messages.value.push({
      sender: currentUid.value,
      content,
      self: true,
      serverSeq: 0,
      serverTime: Date.now(),
      msgType,
      status: 0,
      sendStatus: 'sending',
      localId: lid,
    })
  }

  /**
   * 文件消息发送流程:
   * 1. requestUpload → 获取 fileId + uploadUrl
   * 2. 上传文件到 uploadUrl (由 C++ 层 / 用户手动完成)
   * 3. uploadComplete → 确认上传完成
   * 4. 发送 file 类型消息
   */
  function sendFileMessage(fileName: string, fileSize: number, mimeType: string) {
    if (!activeConv.value) return
    const convId = activeConv.value.conversationId

    // 乐观展示
    const lid = ++localIdCounter
    const fileContent = JSON.stringify({ file_id: 0, file_name: fileName, file_size: fileSize })
    messages.value.push({
      sender: currentUid.value,
      content: fileContent,
      self: true,
      serverSeq: 0,
      serverTime: Date.now(),
      msgType: 5, // FILE
      status: 0,
      sendStatus: 'sending',
      localId: lid,
    })

    // 请求上传
    bridge.send('requestUpload', {
      fileName,
      fileSize,
      mimeType,
      fileHash: '',
      fileType: 'chat',
    })

    bridge.on<{ success: boolean; msg?: string; fileId?: number; uploadUrl?: string; alreadyExists?: boolean }>(
      'requestUploadResult',
      (data) => {
        bridge.off('requestUploadResult')
        const pending = messages.value.find((m) => m.localId === lid)

        if (!data.success) {
          if (pending) pending.sendStatus = 'failed'
          return
        }

        // 确认上传完成 (简化 — 实际需要通过 uploadUrl PUT 文件后再调用)
        if (data.fileId) {
          bridge.send('uploadComplete', { fileId: data.fileId })
        }

        // 发送文件消息
        const content = JSON.stringify({
          file_id: data.fileId || 0,
          file_name: fileName,
          file_size: fileSize,
        })
        bridge.send('sendMessage', {
          to: String(convId),
          content,
        })

        if (pending) {
          pending.content = content
        }
      },
    )
  }

  function recallMessage(serverSeq: number) {
    if (!activeConv.value) return
    bridge.send('recallMessage', {
      conversationId: activeConv.value.conversationId,
      serverSeq,
    })
  }

  function deleteConversation(conversationId: number) {
    bridge.send('deleteConversation', { conversationId })
    conversations.value = conversations.value.filter((c) => c.conversationId !== conversationId)
    if (activeConv.value?.conversationId === conversationId) {
      activeConv.value = null
      messages.value = []
    }
  }

  function muteConversation(conversationId: number, mute: boolean) {
    bridge.send('muteConversation', { conversationId, mute })
    const conv = conversations.value.find((c) => c.conversationId === conversationId)
    if (conv) conv.mute = mute ? 1 : 0
  }

  function pinConversation(conversationId: number, pinned: boolean) {
    bridge.send('pinConversation', { conversationId, pinned })
    const conv = conversations.value.find((c) => c.conversationId === conversationId)
    if (conv) conv.pinned = pinned ? 1 : 0
  }

  return {
    conversations,
    activeConv,
    messages,
    loadingHistory,
    loadingConversations,
    hasMore,
    init,
    loadConversations,
    selectConversation,
    loadMoreHistory,
    sendMessage,
    sendFileMessage,
    recallMessage,
    deleteConversation,
    muteConversation,
    pinConversation,
  }
})
