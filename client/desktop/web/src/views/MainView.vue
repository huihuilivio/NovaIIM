<script setup lang="ts">
import { ref, computed, nextTick, onMounted, watch, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useChatStore, type Conversation } from '@/stores/chat'
import { useConnectionStore } from '@/stores/connection'
import { useContactsStore } from '@/stores/contacts'
import { useGroupsStore } from '@/stores/groups'
import ContactsView from './ContactsView.vue'
import GroupsView from './GroupsView.vue'

const router = useRouter()
const auth = useAuthStore()
const chat = useChatStore()
const conn = useConnectionStore()
const contacts = useContactsStore()
const groups = useGroupsStore()

const chatInput = ref('')
const messagesEl = ref<HTMLDivElement>()
const activeNav = ref<'chat' | 'contacts' | 'groups' | 'settings'>('chat')
const searchKeyword = ref('')

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

function selectConv(conv: Conversation) {
  chat.selectConversation(conv)
}

async function doSend() {
  const text = chatInput.value.trim()
  if (!text || !chat.activeConv) return
  chat.sendMessage(text)
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
}

// 关闭所有上下文菜单
function closeMenus() {
  convMenu.value.show = false
  msgMenu.value.show = false
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
    profileMsg.value = '保存成功'
  } else {
    profileMsg.value = r.msg || '保存失败'
  }
  setTimeout(() => { profileMsg.value = '' }, 3000)
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
}

const currentGroupRole = computed(() => {
  const g = groups.groups.find((g) => g.conversationId === selectedGroupConvId.value)
  return g?.myRole ?? 0
})

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

  document.addEventListener('click', onDocClick)
})

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
            <div class="conv-avatar">
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
          </div>
          <div ref="messagesEl" class="chat-messages" @scroll="handleScroll">
            <div v-if="chat.loadingHistory" class="loading-tip">加载中...</div>
            <div v-if="!chat.hasMore && chat.messages.length > 0" class="loading-tip">没有更多消息</div>
            <div
              v-for="(msg, i) in chat.messages"
              :key="i"
              class="chat-msg"
              :class="{ self: msg.self, recalled: msg.status === 1 }"
              @contextmenu="onMsgContextMenu($event, msg.serverSeq, msg.self)"
            >
              <template v-if="msg.status === 1">
                <div class="recall-tip">消息已撤回</div>
              </template>
              <template v-else>
                <div class="msg-avatar">{{ msg.self ? userInitial : (msg.sender || '?').charAt(0) }}</div>
                <div class="msg-bubble">{{ escapeHtml(msg.content) }}</div>
              </template>
            </div>
          </div>
          <div class="chat-input-area">
            <input v-model="chatInput" type="text" placeholder="输入消息..." @keydown.enter="doSend" />
            <button @click="doSend">发送</button>
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
            <div class="detail-avatar">{{ (contacts.selectedProfile.nickname || contacts.selectedProfile.uid).charAt(0) }}</div>
            <div class="detail-info">
              <div class="detail-name">{{ escapeHtml(contacts.selectedProfile.nickname) }}</div>
              <div class="detail-uid">UID: {{ contacts.selectedProfile.uid }}</div>
              <div class="detail-uid" v-if="contacts.selectedProfile.email">{{ contacts.selectedProfile.email }}</div>
            </div>
          </div>
          <div class="detail-actions">
            <button class="btn-action" @click="contacts.blockFriend(selectedFriendUid)">拉黑</button>
            <button class="btn-action btn-danger-outline" @click="contacts.deleteFriend(selectedFriendUid).then(() => { selectedFriendUid = '' })">删除好友</button>
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
              <div class="detail-name">{{ escapeHtml(groups.currentGroupInfo.name) }}</div>
              <div class="detail-uid">{{ groups.currentGroupInfo.memberCount }} 人</div>
              <div class="detail-uid" v-if="groups.currentGroupInfo.notice">公告: {{ escapeHtml(groups.currentGroupInfo.notice) }}</div>
            </div>
          </div>

          <!-- 成员列表 -->
          <div class="detail-section">
            <div class="detail-section-title">成员列表</div>
            <div class="member-list">
              <div v-for="m in groups.currentMembers" :key="m.userId" class="member-item">
                <div class="member-avatar">{{ (m.nickname || m.uid).charAt(0) }}</div>
                <div class="member-info">
                  <span class="member-name">{{ escapeHtml(m.nickname || m.uid) }}</span>
                  <span v-if="m.role === 2" class="role-tag owner">群主</span>
                  <span v-else-if="m.role === 1" class="role-tag admin">管理员</span>
                </div>
                <div class="member-actions" v-if="currentGroupRole >= 1 && m.role < currentGroupRole">
                  <button class="btn-xs" @click="groups.kickMember(selectedGroupConvId, m.uid)" title="踢出">✕</button>
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
              <button class="btn-action btn-danger-outline" @click="groups.dismissGroup(selectedGroupConvId).then(() => { selectedGroupConvId = 0 })">解散群组</button>
            </template>
            <template v-else>
              <button class="btn-action btn-danger-outline" @click="groups.leaveGroup(selectedGroupConvId).then(() => { selectedGroupConvId = 0 })">退出群组</button>
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
              <span v-if="profileMsg" class="tip-msg" style="margin-left:8px">{{ profileMsg }}</span>
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
</template>
