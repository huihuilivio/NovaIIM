<script setup lang="ts">
import { ref, computed, nextTick, onMounted, watch, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useChatStore, type Conversation, type ChatMessage } from '@/stores/chat'
import { useConnectionStore } from '@/stores/connection'
import { useContactsStore } from '@/stores/contacts'
import { useGroupsStore } from '@/stores/groups'
import ContactsView from './ContactsView.vue'
import GroupsView from './GroupsView.vue'
import EmojiPicker from '@/components/EmojiPicker.vue'
import Toast from '@/components/Toast.vue'
import UserProfileModal from '@/components/UserProfileModal.vue'

const router = useRouter()
const auth = useAuthStore()
const chat = useChatStore()
const conn = useConnectionStore()
const contacts = useContactsStore()
const groups = useGroupsStore()

const chatInput = ref('')
const messagesEl = ref<HTMLDivElement>()
const toastRef = ref<InstanceType<typeof Toast>>()
const activeNav = ref<'chat' | 'contacts' | 'groups' | 'settings'>('chat')
const searchKeyword = ref('')

// Emoji picker
const showEmoji = ref(false)

// 文件发送
const fileInputRef = ref<HTMLInputElement>()

// 会话右键菜单
const convMenu = ref<{ show: boolean; x: number; y: number; conv: Conversation | null }>({
  show: false, x: 0, y: 0, conv: null,
})

// 消息右键菜单
const msgMenu = ref<{ show: boolean; x: number; y: number; seq: number; isSelf: boolean }>({
  show: false, x: 0, y: 0, seq: 0, isSelf: false,
})

// 个人资料编辑
const profileNickname = ref('')
const profileMsg = ref('')

// 选中的好友 (点击好友列表后右面板展示)
const selectedFriendUid = ref('')

// 选中的群组 (点击群组列表后右面板展示)
const selectedGroupConvId = ref(0)

// 群组编辑
const editGroupName = ref('')
const editGroupNotice = ref('')
const showGroupEdit = ref(false)

// 设置项
const notifyEnabled = ref(true)
const notifySound = ref(true)

// 用户资料弹窗
const profileModalUser = ref<{ uid: string; nickname: string; avatar: string; email: string; status: number; createdAt: string } | null>(null)
const profileModalIsFriend = ref(false)

const userInitial = computed(() => (auth.nickname || 'U').charAt(0))
const statusClass = computed(() => 'status-dot ' + conn.state.toLowerCase())
const totalUnread = computed(() =>
  chat.conversations.reduce((sum, c) => sum + c.unreadCount, 0),
)

const filteredConversations = computed(() => {
  if (!searchKeyword.value.trim()) return chat.conversations
  const kw = searchKeyword.value.toLowerCase()
  return chat.conversations.filter((c) => c.name.toLowerCase().includes(kw))
})

// 时间分割线：判断两条消息间是否需要显示时间
function shouldShowTimeDivider(msgs: ChatMessage[], index: number): boolean {
  if (index === 0) return true
  const prev = msgs[index - 1]
  const curr = msgs[index]
  const prevTime = prev.serverTime || 0
  const currTime = curr.serverTime || 0
  if (!prevTime || !currTime) return false
  return currTime - prevTime > 5 * 60 * 1000 // 5分钟间隔
}

function formatMsgTime(epochMs: number): string {
  if (!epochMs) return ''
  const d = new Date(epochMs)
  const now = new Date()
  const hm = `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`
  if (d.toDateString() === now.toDateString()) return hm
  const yesterday = new Date(now)
  yesterday.setDate(yesterday.getDate() - 1)
  if (d.toDateString() === yesterday.toDateString()) return `昨天 ${hm}`
  return `${d.getMonth() + 1}/${d.getDate()} ${hm}`
}

// 消息发送状态文字
function sendStatusText(msg: ChatMessage): string {
  if (!msg.self) return ''
  if (msg.sendStatus === 'sending') return '发送中...'
  if (msg.sendStatus === 'failed') return '发送失败'
  return ''
}

// 消息类型常量 (与 protocol/proto_types.h MsgType 一致)
const MSG_TYPE = {
  TEXT: 1,     // 纯文本 (0 也当作文本，兼容旧消息)
  IMAGE: 2,    // 图片 — content: {"file_id","width","height","thumb"}
  VOICE: 3,    // 语音 — content: {"file_id","duration"}
  VIDEO: 4,    // 视频 — content: {"file_id","duration","width","height","thumb"}
  FILE: 5,     // 文件 — content: {"file_id","file_name","file_size"}
  LOCATION: 6, // 位置 — content: {"lat","lng","name","addr"}
  EMOJI: 7,    // 表情 — content: "emoji:xxx" / "sticker:xxx"
  CARD: 8,     // 名片 — content: {"uid","nickname","avatar"}
  SYSTEM: 9,   // 系统消息
  CUSTOM: 10,  // 自定义
} as const

// 解析 JSON content，失败返回 null
function tryParseJson(content: string): Record<string, unknown> | null {
  try {
    const o = JSON.parse(content)
    return typeof o === 'object' && o !== null ? o : null
  } catch {
    return null
  }
}

// 格式化文件大小
function formatFileSize(bytes: number): string {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
  return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
}

// 格式化语音时长
function formatDuration(seconds: number): string {
  const m = Math.floor(seconds / 60)
  const s = seconds % 60
  return m > 0 ? `${m}:${s.toString().padStart(2, '0')}` : `0:${s.toString().padStart(2, '0')}`
}

function selectConv(conv: Conversation) {
  chat.selectConversation(conv)
}

async function doSend() {
  const text = chatInput.value.trim()
  if (!text || !chat.activeConv) return
  chat.sendMessage(text)
  chatInput.value = ''
  showEmoji.value = false
  await nextTick()
  scrollToBottom()
}

function onEmojiSelect(emoji: string) {
  chatInput.value += emoji
}

function triggerFilePicker() {
  fileInputRef.value?.click()
}

function onFileSelected(e: Event) {
  const input = e.target as HTMLInputElement
  const file = input.files?.[0]
  if (!file) return
  // 通过文件上传流程发送
  chat.sendFileMessage(file.name, file.size, file.type || 'application/octet-stream')
  input.value = ''
  toastRef.value?.show(`正在发送文件 "${file.name}"`, 'info')
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
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;')
}

function formatTime(epochMs: number): string {
  if (!epochMs) return ''
  const d = new Date(epochMs)
  const now = new Date()
  if (d.toDateString() === now.toDateString()) {
    return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`
  }
  return `${d.getMonth() + 1}/${d.getDate()}`
}

function formatLastMsg(conv: Conversation): string {
  if (!conv.lastMsg || !conv.lastMsg.content) return ''
  const prefix = conv.type === 2 && conv.lastMsg.senderNickname
    ? conv.lastMsg.senderNickname + ': '
    : ''
  // 根据消息类型显示简短预览
  const t = conv.lastMsg.msgType
  if (t === MSG_TYPE.IMAGE) return prefix + '[图片]'
  if (t === MSG_TYPE.VOICE) return prefix + '[语音]'
  if (t === MSG_TYPE.VIDEO) return prefix + '[视频]'
  if (t === MSG_TYPE.FILE) {
    const j = tryParseJson(conv.lastMsg.content)
    return prefix + '[文件] ' + ((j as any)?.file_name || '')
  }
  if (t === MSG_TYPE.LOCATION) return prefix + '[位置]'
  if (t === MSG_TYPE.EMOJI) return prefix + conv.lastMsg.content
  if (t === MSG_TYPE.CARD) return prefix + '[名片]'
  if (t === MSG_TYPE.SYSTEM) return conv.lastMsg.content
  return prefix + conv.lastMsg.content
}

function handleScroll(e: Event) {
  const el = e.target as HTMLDivElement
  if (el.scrollTop === 0 && chat.hasMore) {
    chat.loadMoreHistory()
  }
}

// 会话右键菜单
function onConvContextMenu(e: MouseEvent, conv: Conversation) {
  e.preventDefault()
  convMenu.value = { show: true, x: e.clientX, y: e.clientY, conv }
}

function convMenuPin() {
  if (!convMenu.value.conv) return
  chat.pinConversation(convMenu.value.conv.conversationId, !convMenu.value.conv.pinned)
  convMenu.value.show = false
}

function convMenuMute() {
  if (!convMenu.value.conv) return
  chat.muteConversation(convMenu.value.conv.conversationId, !convMenu.value.conv.mute)
  convMenu.value.show = false
}

function convMenuDelete() {
  if (!convMenu.value.conv) return
  chat.deleteConversation(convMenu.value.conv.conversationId)
  convMenu.value.show = false
}

// 消息右键菜单
function onMsgContextMenu(e: MouseEvent, seq: number, isSelf: boolean) {
  e.preventDefault()
  if (seq <= 0) return // 本地消息还没有 serverSeq
  msgMenu.value = { show: true, x: e.clientX, y: e.clientY, seq, isSelf }
}

function msgMenuRecall() {
  chat.recallMessage(msgMenu.value.seq)
  msgMenu.value.show = false
}

function msgMenuCopy(content: string) {
  navigator.clipboard.writeText(content).catch(() => {})
  msgMenu.value.show = false
  toastRef.value?.show('已复制', 'success', 1500)
}

// 关闭所有上下文菜单
function closeMenus() {
  convMenu.value.show = false
  msgMenu.value.show = false
  showEmoji.value = false
}

// 个人资料
function openSettings() {
  activeNav.value = 'settings'
  profileNickname.value = auth.nickname
  profileMsg.value = ''
}

async function saveProfile() {
  const r = await contacts.updateProfile(profileNickname.value.trim(), '')
  if (r.success) {
    auth.nickname = profileNickname.value.trim()
    toastRef.value?.show('保存成功', 'success')
  } else {
    toastRef.value?.show(r.msg || '保存失败', 'error')
  }
}

// 选择好友，展示详情
function selectFriend(uid: string) {
  selectedFriendUid.value = uid
  contacts.getUserProfile(uid)
}

// 选择群组，展示详情
function selectGroup(convId: number) {
  selectedGroupConvId.value = convId
  groups.getGroupInfo(convId)
  groups.getGroupMembers(convId)
  showGroupEdit.value = false
}

const currentGroupRole = computed(() => {
  const g = groups.groups.find((g) => g.conversationId === selectedGroupConvId.value)
  return g?.myRole ?? 0
})

// 群组编辑
function startGroupEdit() {
  if (!groups.currentGroupInfo) return
  editGroupName.value = groups.currentGroupInfo.name
  editGroupNotice.value = groups.currentGroupInfo.notice || ''
  showGroupEdit.value = true
}

async function saveGroupEdit() {
  if (!selectedGroupConvId.value) return
  const r = await groups.updateGroup(selectedGroupConvId.value, editGroupName.value.trim(), editGroupNotice.value.trim())
  if (r.success) {
    showGroupEdit.value = false
    toastRef.value?.show('群组信息已更新', 'success')
  } else {
    toastRef.value?.show(r.msg || '更新失败', 'error')
  }
}

// 用户资料弹窗
function showUserProfile(uid: string) {
  const isFriend = contacts.friends.some((f) => f.uid === uid)
  contacts.getUserProfile(uid)
  profileModalIsFriend.value = isFriend
  // 等待 profile 加载后显示弹窗
  const unwatch = watch(() => contacts.selectedProfile, (p) => {
    if (p && p.uid === uid) {
      profileModalUser.value = p
      unwatch()
    }
  }, { immediate: true })
}

watch(() => chat.messages.length, async () => {
  await nextTick()
  scrollToBottom()
})

function onDocClick() {
  closeMenus()
}

onMounted(() => {
  conn.init()
  chat.init(auth.uid)
  contacts.init()
  groups.init()

  // 登录后主动拉取数据，服务端同步后UI更新
  chat.loadConversations()
  contacts.loadFriendList()
  contacts.loadFriendRequests()
  groups.loadGroups()

  // 加载设置
  try {
    notifyEnabled.value = localStorage.getItem('nova_notify') !== 'false'
    notifySound.value = localStorage.getItem('nova_sound') !== 'false'
  } catch { /* ignore */ }

  document.addEventListener('click', onDocClick)
})

function saveSetting(key: string, value: boolean) {
  try { localStorage.setItem(key, String(value)) } catch { /* ignore */ }
}

onUnmounted(() => {
  document.removeEventListener('click', onDocClick)
})
</script>

<template>
  <div class="main-page" @click="closeMenus">
    <!-- 侧边栏 -->
    <div class="sidebar">
      <div class="avatar" @click="openSettings" title="个人设置" style="cursor:pointer">{{ userInitial }}</div>
      <button class="nav-btn" :class="{ active: activeNav === 'chat' }" title="消息" @click="activeNav = 'chat'">
        💬
        <span v-if="totalUnread > 0" class="nav-badge">{{ totalUnread > 99 ? '99+' : totalUnread }}</span>
      </button>
      <button class="nav-btn" :class="{ active: activeNav === 'contacts' }" title="联系人" @click="activeNav = 'contacts'; selectedFriendUid = ''">
        👤
        <span v-if="contacts.pendingCount > 0" class="nav-badge">{{ contacts.pendingCount }}</span>
      </button>
      <button class="nav-btn" :class="{ active: activeNav === 'groups' }" title="群组" @click="activeNav = 'groups'; selectedGroupConvId = 0">
        👥
      </button>
      <div class="spacer"></div>
      <button class="nav-btn" title="设置" @click="openSettings">⚙</button>
    </div>

    <!-- ==================== 消息页 ==================== -->
    <template v-if="activeNav === 'chat'">
      <div class="conv-list">
        <div class="conv-search">
          <input v-model="searchKeyword" type="text" placeholder="🔍 搜索会话" />
        </div>
        <div class="conv-items">
          <div v-if="chat.loadingConversations" class="empty-tip">加载中...</div>
          <div v-else-if="filteredConversations.length === 0" class="empty-tip">暂无会话</div>
          <div
            v-for="conv in filteredConversations"
            :key="conv.conversationId"
            class="conv-item"
            :class="{ active: chat.activeConv?.conversationId === conv.conversationId, pinned: conv.pinned }"
            @click="selectConv(conv)"
            @contextmenu="onConvContextMenu($event, conv)"
          >
            <div class="conv-avatar" :class="{ 'group-conv-avatar': conv.type === 2 }">
              {{ conv.name.charAt(0) }}
              <span v-if="conv.unreadCount > 0" class="unread-badge">
                {{ conv.unreadCount > 99 ? '99+' : conv.unreadCount }}
              </span>
            </div>
            <div class="conv-info">
              <div class="conv-header">
                <span class="conv-name">
                  <span v-if="conv.pinned" class="pin-icon">📌</span>
                  <span v-if="conv.mute" class="mute-icon">🔇</span>
                  {{ escapeHtml(conv.name) }}
                </span>
                <span class="conv-time">{{ formatTime(conv.lastMsg?.serverTime) }}</span>
              </div>
              <div class="conv-last-msg">{{ escapeHtml(formatLastMsg(conv)) }}</div>
            </div>
          </div>
        </div>
      </div>

      <div class="chat-area">
        <template v-if="chat.activeConv">
          <div class="chat-header">
            <span>{{ escapeHtml(chat.activeConv.name) }}</span>
            <span v-if="chat.activeConv.type === 2" class="chat-member-count">(群聊)</span>
          </div>
          <div ref="messagesEl" class="chat-messages" @scroll="handleScroll">
            <div v-if="chat.loadingHistory" class="loading-tip">加载中...</div>
            <div v-if="!chat.hasMore && chat.messages.length > 0" class="loading-tip">没有更多消息</div>
            <template v-for="(msg, i) in chat.messages" :key="i">
              <!-- 时间分割线 -->
              <div v-if="shouldShowTimeDivider(chat.messages, i)" class="time-divider">
                <span>{{ formatMsgTime(msg.serverTime) }}</span>
              </div>
              <div
                class="chat-msg"
                :class="{ self: msg.self, recalled: msg.status === 1 }"
                @contextmenu="onMsgContextMenu($event, msg.serverSeq, msg.self)"
              >
                <template v-if="msg.status === 1">
                  <div class="recall-tip">消息已撤回</div>
                </template>
                <template v-else>
                  <div class="msg-avatar" @click="showUserProfile(msg.sender)" style="cursor:pointer">
                    {{ msg.self ? userInitial : (msg.sender || '?').charAt(0) }}
                  </div>
                  <div class="msg-content-wrap">
                    <!-- 文本消息 (type 0 或 1) -->
                    <div v-if="!msg.msgType || msg.msgType === MSG_TYPE.TEXT" class="msg-bubble">{{ escapeHtml(msg.content) }}</div>

                    <!-- 图片消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.IMAGE" class="msg-bubble msg-image">
                      <template v-if="tryParseJson(msg.content)">
                        <img
                          :src="(tryParseJson(msg.content) as any).thumb ? 'data:image/jpeg;base64,' + (tryParseJson(msg.content) as any).thumb : ''"
                          :alt="'图片'"
                          class="msg-img"
                          @click="showUserProfile('')"
                        />
                        <div class="msg-img-size">{{ (tryParseJson(msg.content) as any).width }}×{{ (tryParseJson(msg.content) as any).height }}</div>
                      </template>
                      <template v-else>
                        <span class="msg-file-icon">🖼️</span> [图片]
                      </template>
                    </div>

                    <!-- 语音消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.VOICE" class="msg-bubble msg-voice">
                      <span class="msg-file-icon">🎤</span>
                      <span>语音消息</span>
                      <span v-if="tryParseJson(msg.content)" class="msg-voice-dur">{{ formatDuration((tryParseJson(msg.content) as any).duration || 0) }}</span>
                    </div>

                    <!-- 视频消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.VIDEO" class="msg-bubble msg-video">
                      <span class="msg-file-icon">🎬</span>
                      <span>视频</span>
                      <span v-if="tryParseJson(msg.content)" class="msg-voice-dur">{{ formatDuration((tryParseJson(msg.content) as any).duration || 0) }}</span>
                    </div>

                    <!-- 文件消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.FILE" class="msg-bubble msg-file-bubble">
                      <template v-if="tryParseJson(msg.content)">
                        <span class="msg-file-icon">📄</span>
                        <div class="msg-file-info">
                          <div class="msg-file-name">{{ escapeHtml((tryParseJson(msg.content) as any).file_name || '未知文件') }}</div>
                          <div class="msg-file-size">{{ formatFileSize((tryParseJson(msg.content) as any).file_size || 0) }}</div>
                        </div>
                      </template>
                      <template v-else>
                        <span class="msg-file-icon">📄</span> [文件]
                      </template>
                    </div>

                    <!-- 位置消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.LOCATION" class="msg-bubble msg-location">
                      <span class="msg-file-icon">📍</span>
                      <template v-if="tryParseJson(msg.content)">
                        <div class="msg-file-info">
                          <div class="msg-file-name">{{ escapeHtml((tryParseJson(msg.content) as any).name || '位置') }}</div>
                          <div class="msg-file-size">{{ escapeHtml((tryParseJson(msg.content) as any).addr || '') }}</div>
                        </div>
                      </template>
                      <template v-else>
                        [位置]
                      </template>
                    </div>

                    <!-- Emoji 消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.EMOJI" class="msg-bubble msg-emoji-large">
                      {{ msg.content }}
                    </div>

                    <!-- 名片消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.CARD" class="msg-bubble msg-card" @click="tryParseJson(msg.content) && showUserProfile((tryParseJson(msg.content) as any).uid || '')">
                      <template v-if="tryParseJson(msg.content)">
                        <div class="msg-card-avatar">{{ ((tryParseJson(msg.content) as any).nickname || '?').charAt(0) }}</div>
                        <div class="msg-file-info">
                          <div class="msg-file-name">{{ escapeHtml((tryParseJson(msg.content) as any).nickname || '') }}</div>
                          <div class="msg-file-size">个人名片</div>
                        </div>
                      </template>
                      <template v-else>
                        <span class="msg-file-icon">👤</span> [名片]
                      </template>
                    </div>

                    <!-- 系统消息 -->
                    <div v-else-if="msg.msgType === MSG_TYPE.SYSTEM" class="msg-system-text">
                      {{ escapeHtml(msg.content) }}
                    </div>

                    <!-- 未知类型 fallback -->
                    <div v-else class="msg-bubble">{{ escapeHtml(msg.content) }}</div>

                    <div v-if="sendStatusText(msg)" class="msg-status" :class="{ 'msg-status-failed': msg.sendStatus === 'failed' }">
                      {{ sendStatusText(msg) }}
                    </div>
                  </div>
                </template>
              </div>
            </template>
          </div>
          <div class="chat-input-area">
            <div class="chat-toolbar">
              <button class="toolbar-btn" title="表情" @click.stop="showEmoji = !showEmoji">😊</button>
              <button class="toolbar-btn" title="文件" @click="triggerFilePicker">📎</button>
              <input ref="fileInputRef" type="file" style="display:none" @change="onFileSelected" />
            </div>
            <div class="chat-input-row">
              <input v-model="chatInput" type="text" placeholder="输入消息..." @keydown.enter="doSend" />
              <button @click="doSend">发送</button>
            </div>
            <!-- Emoji Picker -->
            <div v-if="showEmoji" class="emoji-picker-anchor" @click.stop>
              <EmojiPicker @select="onEmojiSelect" />
            </div>
          </div>
        </template>
        <div v-else class="chat-placeholder">选择一个会话开始聊天</div>
      </div>
    </template>

    <!-- ==================== 联系人页 ==================== -->
    <template v-if="activeNav === 'contacts'">
      <div class="side-panel">
        <ContactsView @select="selectFriend" />
      </div>
      <div class="chat-area">
        <!-- 好友详情面板 -->
        <div v-if="selectedFriendUid && contacts.selectedProfile" class="detail-panel">
          <div class="detail-header">
            <div class="detail-avatar" @click="showUserProfile(selectedFriendUid)" style="cursor:pointer">
              {{ (contacts.selectedProfile.nickname || contacts.selectedProfile.uid).charAt(0) }}
            </div>
            <div class="detail-info">
              <div class="detail-name">{{ escapeHtml(contacts.selectedProfile.nickname) }}</div>
              <div class="detail-uid">UID: {{ contacts.selectedProfile.uid }}</div>
              <div class="detail-uid" v-if="contacts.selectedProfile.email">{{ contacts.selectedProfile.email }}</div>
            </div>
          </div>
          <div class="detail-actions">
            <button class="btn-action" @click="contacts.blockFriend(selectedFriendUid).then(r => toastRef?.show(r.success ? '已拉黑' : (r.msg || '操作失败'), r.success ? 'success' : 'error'))">拉黑</button>
            <button class="btn-action btn-danger-outline" @click="contacts.deleteFriend(selectedFriendUid).then(() => { selectedFriendUid = ''; toastRef?.show('已删除', 'success') })">删除好友</button>
          </div>
        </div>
        <div v-else class="chat-placeholder">选择好友查看详情</div>
      </div>
    </template>

    <!-- ==================== 群组页 ==================== -->
    <template v-if="activeNav === 'groups'">
      <div class="side-panel">
        <GroupsView @select="selectGroup" />
      </div>
      <div class="chat-area">
        <!-- 群组详情面板 -->
        <div v-if="selectedGroupConvId && groups.currentGroupInfo" class="detail-panel">
          <div class="detail-header">
            <div class="detail-avatar group-avatar">{{ (groups.currentGroupInfo.name || '群').charAt(0) }}</div>
            <div class="detail-info">
              <template v-if="!showGroupEdit">
                <div class="detail-name">{{ escapeHtml(groups.currentGroupInfo.name) }}</div>
                <div class="detail-uid">{{ groups.currentGroupInfo.memberCount }} 人</div>
                <div class="detail-uid" v-if="groups.currentGroupInfo.notice">公告: {{ escapeHtml(groups.currentGroupInfo.notice) }}</div>
              </template>
              <template v-else>
                <div class="edit-field">
                  <label>群名</label>
                  <input v-model="editGroupName" type="text" placeholder="群组名称" />
                </div>
                <div class="edit-field">
                  <label>公告</label>
                  <textarea v-model="editGroupNotice" placeholder="群公告" rows="3"></textarea>
                </div>
              </template>
            </div>
          </div>

          <!-- 编辑按钮 -->
          <div class="detail-actions" v-if="currentGroupRole >= 1">
            <template v-if="!showGroupEdit">
              <button class="btn-action" @click="startGroupEdit">编辑群信息</button>
            </template>
            <template v-else>
              <button class="btn-action" @click="saveGroupEdit">保存</button>
              <button class="btn-action" @click="showGroupEdit = false">取消</button>
            </template>
          </div>

          <!-- 成员列表 -->
          <div class="detail-section">
            <div class="detail-section-title">成员列表</div>
            <div class="member-list">
              <div v-for="m in groups.currentMembers" :key="m.userId" class="member-item">
                <div class="member-avatar" @click="showUserProfile(m.uid)" style="cursor:pointer">
                  {{ (m.nickname || m.uid).charAt(0) }}
                </div>
                <div class="member-info">
                  <span class="member-name">{{ escapeHtml(m.nickname || m.uid) }}</span>
                  <span v-if="m.role === 2" class="role-tag owner">群主</span>
                  <span v-else-if="m.role === 1" class="role-tag admin">管理员</span>
                </div>
                <div class="member-actions" v-if="currentGroupRole >= 1 && m.role < currentGroupRole">
                  <button class="btn-xs" @click="groups.kickMember(selectedGroupConvId, m.uid).then(r => toastRef?.show(r.success ? '已踢出' : (r.msg || '操作失败'), r.success ? 'success' : 'error'))" title="踢出">✕</button>
                  <button v-if="currentGroupRole === 2" class="btn-xs" @click="groups.setMemberRole(selectedGroupConvId, m.uid, m.role === 1 ? 0 : 1)" :title="m.role === 1 ? '取消管理' : '设为管理'">
                    {{ m.role === 1 ? '↓' : '↑' }}
                  </button>
                </div>
              </div>
            </div>
          </div>

          <!-- 群组操作 -->
          <div class="detail-actions">
            <template v-if="currentGroupRole === 2">
              <button class="btn-action btn-danger-outline" @click="groups.dismissGroup(selectedGroupConvId).then(() => { selectedGroupConvId = 0; toastRef?.show('群组已解散', 'success') })">解散群组</button>
            </template>
            <template v-else>
              <button class="btn-action btn-danger-outline" @click="groups.leaveGroup(selectedGroupConvId).then(() => { selectedGroupConvId = 0; toastRef?.show('已退出群组', 'success') })">退出群组</button>
            </template>
          </div>
        </div>
        <div v-else class="chat-placeholder">选择群组查看详情</div>
      </div>
    </template>

    <!-- ==================== 设置页 ==================== -->
    <template v-if="activeNav === 'settings'">
      <div class="side-panel">
        <div class="settings-panel">
          <div class="detail-section-title" style="padding: 16px">个人设置</div>
          <div class="settings-form">
            <div class="settings-item">
              <label>UID</label>
              <div class="settings-value">{{ auth.uid }}</div>
            </div>
            <div class="settings-item">
              <label>昵称</label>
              <input v-model="profileNickname" type="text" />
            </div>
            <div class="settings-item">
              <button class="btn-action" @click="saveProfile">保存</button>
            </div>
          </div>

          <div class="detail-section-title" style="padding: 16px 16px 8px">通知设置</div>
          <div class="settings-form">
            <div class="settings-item">
              <label>通知</label>
              <label class="toggle">
                <input type="checkbox" v-model="notifyEnabled" @change="saveSetting('nova_notify', notifyEnabled)" />
                <span class="toggle-slider"></span>
              </label>
            </div>
            <div class="settings-item">
              <label>声音</label>
              <label class="toggle">
                <input type="checkbox" v-model="notifySound" @change="saveSetting('nova_sound', notifySound)" />
                <span class="toggle-slider"></span>
              </label>
            </div>
          </div>

          <div class="detail-section-title" style="padding: 16px 16px 8px">关于</div>
          <div class="settings-form">
            <div class="settings-item">
              <label>版本</label>
              <div class="settings-value">NovaIIM v1.0.0</div>
            </div>
          </div>
        </div>
      </div>
      <div class="chat-area">
        <div class="chat-placeholder">
          <div style="text-align:center">
            <div class="detail-avatar" style="margin: 0 auto 12px; font-size:32px; width:72px; height:72px; line-height:72px">{{ userInitial }}</div>
            <div style="font-size:18px; color:var(--text-primary)">{{ escapeHtml(auth.nickname) }}</div>
            <div style="color:var(--text-secondary); margin-top:4px">{{ auth.uid }}</div>
            <button class="btn-action btn-danger-outline" style="margin-top:24px" @click="handleLogout">退出登录</button>
          </div>
        </div>
      </div>
    </template>
  </div>

  <!-- 会话右键菜单 -->
  <div v-if="convMenu.show" class="context-menu" :style="{ left: convMenu.x + 'px', top: convMenu.y + 'px' }" @click.stop>
    <div class="ctx-item" @click="convMenuPin">{{ convMenu.conv?.pinned ? '取消置顶' : '置顶' }}</div>
    <div class="ctx-item" @click="convMenuMute">{{ convMenu.conv?.mute ? '取消免打扰' : '免打扰' }}</div>
    <div class="ctx-item ctx-danger" @click="convMenuDelete">删除会话</div>
  </div>

  <!-- 消息右键菜单 -->
  <div v-if="msgMenu.show" class="context-menu" :style="{ left: msgMenu.x + 'px', top: msgMenu.y + 'px' }" @click.stop>
    <div class="ctx-item" @click="msgMenuCopy(chat.messages.find(m => m.serverSeq === msgMenu.seq)?.content || '')">复制</div>
    <div v-if="msgMenu.isSelf" class="ctx-item ctx-danger" @click="msgMenuRecall">撤回</div>
  </div>

  <!-- 状态栏 -->
  <div class="status-bar">
    <span :class="statusClass"></span>
    <span>{{ conn.label }}</span>
  </div>

  <!-- Toast 通知 -->
  <Toast ref="toastRef" />

  <!-- 用户资料弹窗 -->
  <UserProfileModal
    v-if="profileModalUser"
    :profile="profileModalUser"
    :is-friend="profileModalIsFriend"
    @close="profileModalUser = null"
  />
</template>
