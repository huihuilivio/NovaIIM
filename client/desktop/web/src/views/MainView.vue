<script setup lang="ts">
import { ref, computed, nextTick, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useChatStore } from '@/stores/chat'
import { useConnectionStore } from '@/stores/connection'
import type { Conversation } from '@/stores/chat'

const router = useRouter()
const auth = useAuthStore()
const chat = useChatStore()
const conn = useConnectionStore()

const chatInput = ref('')
const messagesEl = ref<HTMLDivElement>()

const userInitial = computed(() => (auth.nickname || 'U').charAt(0))

const statusClass = computed(() => {
  return 'status-dot ' + conn.state.toLowerCase()
})

function selectConv(conv: Conversation) {
  chat.selectConversation(conv)
}

async function doSend() {
  const text = chatInput.value.trim()
  if (!text || !chat.activeConv) return

  chat.sendMessage(text, auth.uid)
  chatInput.value = ''

  await nextTick()
  scrollToBottom()
}

function scrollToBottom() {
  if (messagesEl.value) {
    messagesEl.value.scrollTop = messagesEl.value.scrollHeight
  }
}

function handleLogout() {
  auth.logout()
  router.push('/login')
}

function escapeHtml(str: string): string {
  if (!str) return ''
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
}

onMounted(() => {
  conn.init()
  chat.init()
})
</script>

<template>
  <div class="main-page">
    <!-- 侧边栏 -->
    <div class="sidebar">
      <div class="avatar">{{ userInitial }}</div>
      <button class="nav-btn active" title="消息">💬</button>
      <button class="nav-btn" title="联系人">👤</button>
      <button class="nav-btn" title="群组">👥</button>
      <div class="spacer"></div>
      <button class="nav-btn" title="退出" @click="handleLogout">⚙</button>
    </div>

    <!-- 会话列表 -->
    <div class="conv-list">
      <div class="conv-search">
        <input type="text" placeholder="🔍 搜索" />
      </div>
      <div class="conv-items">
        <div
          v-for="conv in chat.conversations"
          :key="conv.id"
          class="conv-item"
          :class="{ active: chat.activeConv?.id === conv.id }"
          @click="selectConv(conv)"
        >
          <div class="conv-avatar">{{ conv.name.charAt(0) }}</div>
          <div class="conv-info">
            <div class="conv-header">
              <span class="conv-name">{{ escapeHtml(conv.name) }}</span>
              <span class="conv-time">{{ escapeHtml(conv.time) }}</span>
            </div>
            <div class="conv-last-msg">{{ escapeHtml(conv.lastMsg) }}</div>
          </div>
        </div>
      </div>
    </div>

    <!-- 聊天区 -->
    <div class="chat-area">
      <template v-if="chat.activeConv">
        <div class="chat-header">{{ escapeHtml(chat.activeConv.name) }}</div>
        <div ref="messagesEl" class="chat-messages">
          <div
            v-for="(msg, i) in chat.messages"
            :key="i"
            class="chat-msg"
            :class="{ self: msg.self }"
          >
            <div class="msg-avatar">
              {{ msg.self ? userInitial : (chat.activeConv?.name || '?').charAt(0) }}
            </div>
            <div class="msg-bubble">{{ escapeHtml(msg.content) }}</div>
          </div>
        </div>
        <div class="chat-input-area">
          <input
            v-model="chatInput"
            type="text"
            placeholder="输入消息..."
            @keydown.enter="doSend"
          />
          <button @click="doSend">发送</button>
        </div>
      </template>
      <div v-else class="chat-placeholder">选择一个会话开始聊天</div>
    </div>
  </div>

  <!-- 状态栏 -->
  <div class="status-bar">
    <span :class="statusClass"></span>
    <span>{{ conn.label }}</span>
  </div>
</template>
