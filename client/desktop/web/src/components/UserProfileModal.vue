<script setup lang="ts">
import { useContactsStore, type UserProfile } from '@/stores/contacts'
import { useChatStore } from '@/stores/chat'

const props = defineProps<{
  profile: UserProfile
  isFriend?: boolean
}>()

const emit = defineEmits<{ close: [] }>()

const contacts = useContactsStore()
const chat = useChatStore()

function startChat() {
  // 查找与该好友的私聊会话
  const conv = chat.conversations.find(
    (c) => c.type === 1 && c.name === (props.profile.nickname || props.profile.uid),
  )
  if (conv) {
    chat.selectConversation(conv)
  }
  emit('close')
}

function escapeHtml(str: string): string {
  if (!str) return ''
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}
</script>

<template>
  <Teleport to="body">
    <div class="modal-overlay" @click="emit('close')">
      <div class="modal-card profile-modal" @click.stop>
        <button class="modal-close" @click="emit('close')">✕</button>
        <div class="profile-header">
          <div class="profile-avatar-lg">{{ (profile.nickname || profile.uid).charAt(0) }}</div>
          <div class="profile-info">
            <div class="profile-name">{{ escapeHtml(profile.nickname) }}</div>
            <div class="profile-uid">UID: {{ profile.uid }}</div>
            <div class="profile-email" v-if="profile.email">{{ profile.email }}</div>
          </div>
        </div>
        <div class="profile-actions" v-if="isFriend">
          <button class="btn-action" @click="startChat">发消息</button>
          <button class="btn-action btn-danger-outline" @click="contacts.deleteFriend(profile.uid); emit('close')">删除好友</button>
        </div>
        <div class="profile-actions" v-else>
          <button class="btn-action" @click="contacts.addFriend(profile.uid, ''); emit('close')">添加好友</button>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.modal-overlay {
  position: fixed;
  inset: 0;
  z-index: 2000;
  background: rgba(0, 0, 0, 0.4);
  display: flex;
  align-items: center;
  justify-content: center;
}

.modal-card {
  position: relative;
  background: var(--bg-primary, #fff);
  border-radius: 12px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
  padding: 32px;
  min-width: 360px;
  max-width: 420px;
}

.modal-close {
  position: absolute;
  top: 12px;
  right: 12px;
  width: 28px;
  height: 28px;
  border: none;
  background: none;
  cursor: pointer;
  font-size: 16px;
  color: var(--text-secondary, #909399);
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.15s;
}

.modal-close:hover {
  background: var(--bg-secondary, #f5f7fa);
  color: var(--text-primary, #303133);
}

.profile-header {
  display: flex;
  align-items: center;
  gap: 16px;
  margin-bottom: 24px;
}

.profile-avatar-lg {
  width: 64px;
  height: 64px;
  min-width: 64px;
  border-radius: 50%;
  background: var(--primary, #409eff);
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 28px;
  font-weight: 600;
}

.profile-info {
  flex: 1;
  min-width: 0;
}

.profile-name {
  font-size: 18px;
  font-weight: 600;
  color: var(--text-primary, #303133);
}

.profile-uid {
  font-size: 13px;
  color: var(--text-secondary, #909399);
  margin-top: 4px;
}

.profile-email {
  font-size: 13px;
  color: var(--text-secondary, #909399);
  margin-top: 2px;
}

.profile-actions {
  display: flex;
  gap: 8px;
  padding-top: 16px;
  border-top: 1px solid var(--border-light, #e4e7ed);
}
</style>
