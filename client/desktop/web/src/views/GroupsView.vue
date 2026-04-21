<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useGroupsStore } from '@/stores/groups'

const emit = defineEmits<{ select: [conversationId: number] }>()

const groups = useGroupsStore()
const showCreate = ref(false)
const newGroupName = ref('')
const createMsg = ref('')

onMounted(() => {
  // init 已在 MainView 完成，此处只刷新数据
  groups.loadGroups()
})

async function doCreateGroup() {
  if (!newGroupName.value.trim()) return
  const r = await groups.createGroup(newGroupName.value.trim(), '', [])
  if (r.success) {
    showCreate.value = false
    newGroupName.value = ''
  }
  createMsg.value = r.success ? '' : (r.msg || '创建失败')
}

async function doLeave(conversationId: number) {
  if (confirm('确定退出该群组？')) {
    await groups.leaveGroup(conversationId)
  }
}

async function doDismiss(conversationId: number) {
  if (confirm('确定解散该群组？此操作不可撤销。')) {
    await groups.dismissGroup(conversationId)
  }
}

function roleLabel(role: number): string {
  if (role === 2) return '群主'
  if (role === 1) return '管理员'
  return '成员'
}

function escapeHtml(str: string): string {
  if (!str) return ''
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}
</script>

<template>
  <div class="groups-panel">
    <!-- 搜索 + 创建 -->
    <div class="panel-search">
      <button class="btn-create" @click="showCreate = !showCreate">+ 创建群组</button>
    </div>

    <!-- 创建群组表单 -->
    <div v-if="showCreate" class="create-form">
      <input
        v-model="newGroupName"
        type="text"
        placeholder="群组名称"
        @keydown.enter="doCreateGroup"
      />
      <button class="btn-sm btn-primary-sm" @click="doCreateGroup">创建</button>
      <span v-if="createMsg" class="tip-msg">{{ createMsg }}</span>
    </div>

    <!-- 群组列表 -->
    <div class="panel-list">
      <div v-if="groups.loadingGroups" class="empty-tip">加载中...</div>
      <div v-else-if="groups.groups.length === 0" class="empty-tip">暂无群组</div>
      <div v-for="g in groups.groups" :key="g.conversationId" class="contact-item clickable" @click="emit('select', g.conversationId)">
        <div class="contact-avatar group-avatar">{{ (g.name || '群').charAt(0) }}</div>
        <div class="contact-info">
          <div class="contact-name">{{ escapeHtml(g.name) }}</div>
          <div class="contact-uid">
            {{ g.memberCount }} 人 · {{ roleLabel(g.myRole) }}
          </div>
        </div>
        <button
          v-if="g.myRole === 2"
          class="btn-sm btn-danger"
          @click.stop="doDismiss(g.conversationId)"
          title="解散群组"
        >解散</button>
        <button
          v-else
          class="btn-sm btn-danger"
          @click.stop="doLeave(g.conversationId)"
          title="退出群组"
        >退出</button>
      </div>
    </div>
  </div>
</template>
