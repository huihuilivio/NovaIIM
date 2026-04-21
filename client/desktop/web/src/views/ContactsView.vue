<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useContactsStore } from '@/stores/contacts'

const emit = defineEmits<{ select: [uid: string] }>()

const contacts = useContactsStore()
const tab = ref<'friends' | 'requests' | 'search'>('friends')
const searchKeyword = ref('')
const addRemark = ref('')
const addMsg = ref('')

onMounted(() => {
  // init 已在 MainView 完成，此处只刷新数据
  contacts.loadFriendList()
  contacts.loadFriendRequests()
})

function doSearch() {
  tab.value = 'search'
  contacts.searchUser(searchKeyword.value)
}

async function doAddFriend(targetUid: string) {
  const r = await contacts.addFriend(targetUid, addRemark.value)
  addMsg.value = r.success ? '好友申请已发送' : (r.msg || '发送失败')
  addRemark.value = ''
  setTimeout(() => { addMsg.value = '' }, 3000)
}

async function doAccept(requestId: number) {
  await contacts.handleRequest(requestId, 1)
}

async function doReject(requestId: number) {
  await contacts.handleRequest(requestId, 2)
}

async function doDelete(uid: string) {
  if (confirm('确定删除该好友？')) {
    await contacts.deleteFriend(uid)
  }
}

function escapeHtml(str: string): string {
  if (!str) return ''
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}
</script>

<template>
  <div class="contacts-panel">
    <!-- 搜索栏 -->
    <div class="panel-search">
      <input
        v-model="searchKeyword"
        type="text"
        placeholder="🔍 搜索用户"
        @keydown.enter="doSearch"
      />
    </div>

    <!-- Tab 切换 -->
    <div class="panel-tabs">
      <button :class="{ active: tab === 'friends' }" @click="tab = 'friends'">
        好友
      </button>
      <button :class="{ active: tab === 'requests' }" @click="tab = 'requests'; contacts.loadFriendRequests()">
        申请
        <span v-if="contacts.pendingCount > 0" class="badge">{{ contacts.pendingCount }}</span>
      </button>
      <button :class="{ active: tab === 'search' }" @click="tab = 'search'">
        搜索
      </button>
    </div>

    <!-- 好友列表 -->
    <div v-if="tab === 'friends'" class="panel-list">
      <div v-if="contacts.loadingFriends" class="empty-tip">加载中...</div>
      <div v-else-if="contacts.friends.length === 0" class="empty-tip">暂无好友</div>
      <div v-for="f in contacts.friends" :key="f.uid" class="contact-item clickable" @click="emit('select', f.uid)">
        <div class="contact-avatar">{{ (f.nickname || f.uid).charAt(0) }}</div>
        <div class="contact-info">
          <div class="contact-name">{{ escapeHtml(f.nickname || f.uid) }}</div>
          <div class="contact-uid">UID: {{ f.uid }}</div>
        </div>
        <button class="btn-sm btn-danger" @click="doDelete(f.uid)" title="删除">✕</button>
      </div>
    </div>

    <!-- 好友申请 -->
    <div v-if="tab === 'requests'" class="panel-list">
      <div v-if="contacts.friendRequests.length === 0" class="empty-tip">暂无好友申请</div>
      <div v-for="r in contacts.friendRequests" :key="r.requestId" class="contact-item">
        <div class="contact-avatar">{{ (r.fromNickname || r.fromUid).charAt(0) }}</div>
        <div class="contact-info">
          <div class="contact-name">{{ escapeHtml(r.fromNickname || r.fromUid) }}</div>
          <div class="contact-uid" v-if="r.remark">{{ escapeHtml(r.remark) }}</div>
        </div>
        <div v-if="r.status === 0" class="request-actions">
          <button class="btn-sm btn-accept" @click="doAccept(r.requestId)">接受</button>
          <button class="btn-sm btn-reject" @click="doReject(r.requestId)">拒绝</button>
        </div>
        <span v-else-if="r.status === 1" class="request-status accepted">已同意</span>
        <span v-else class="request-status rejected">已拒绝</span>
      </div>
    </div>

    <!-- 搜索结果 -->
    <div v-if="tab === 'search'" class="panel-list">
      <div v-if="contacts.searchResults.length === 0 && searchKeyword" class="empty-tip">
        未找到用户
      </div>
      <div v-for="u in contacts.searchResults" :key="u.uid" class="contact-item">
        <div class="contact-avatar">{{ (u.nickname || u.uid).charAt(0) }}</div>
        <div class="contact-info">
          <div class="contact-name">{{ escapeHtml(u.nickname || u.uid) }}</div>
          <div class="contact-uid">UID: {{ u.uid }}</div>
        </div>
        <button class="btn-sm btn-primary-sm" @click="doAddFriend(u.uid)">添加</button>
      </div>
      <div v-if="addMsg" class="tip-msg">{{ addMsg }}</div>
    </div>
  </div>
</template>
