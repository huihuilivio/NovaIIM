import { defineStore } from 'pinia'
import { ref } from 'vue'
import { bridge } from '@/bridge'

export interface MyGroup {
  conversationId: number
  name: string
  avatar: string
  memberCount: number
  myRole: number // 0=成员 1=管理员 2=群主
}

export interface GroupInfo {
  conversationId: number
  name: string
  avatar: string
  ownerId: number
  notice: string
  memberCount: number
  createdAt: string
}

export interface GroupMember {
  userId: number
  uid: string
  nickname: string
  avatar: string
  role: number
  joinedAt: string
}

export interface GroupNotification {
  conversationId: number
  notifyType: number // 1=创建 2=解散 3=加入 4=退出 5=踢出 6=信息变更 7=角色变更
  operatorId: number
  targetIds: number[]
  data: string
}

export const useGroupsStore = defineStore('groups', () => {
  const groups = ref<MyGroup[]>([])
  const currentGroupInfo = ref<GroupInfo | null>(null)
  const currentMembers = ref<GroupMember[]>([])
  const loadingGroups = ref(false)
  let initialized = false

  function init() {
    if (initialized) return
    initialized = true
    bridge.on<{ success: boolean; groups: MyGroup[] }>('myGroups', (data) => {
      loadingGroups.value = false
      if (data.success) {
        groups.value = data.groups
      }
    })

    bridge.on<GroupInfo & { success: boolean }>('groupInfo', (data) => {
      if (data.success) {
        currentGroupInfo.value = data
      }
    })

    bridge.on<{ success: boolean; members: GroupMember[] }>('groupMembers', (data) => {
      if (data.success) {
        currentMembers.value = data.members
      }
    })

    bridge.on<GroupNotification>('groupNotify', (data) => {
      // 群组变更通知 — 刷新列表
      if ([1, 2, 3, 4, 5].includes(data.notifyType)) {
        loadGroups()
      }
    })
  }

  function loadGroups() {
    loadingGroups.value = true
    bridge.send('getMyGroups')
  }

  function getGroupInfo(conversationId: number) {
    bridge.send('getGroupInfo', { conversationId })
  }

  function getGroupMembers(conversationId: number) {
    bridge.send('getGroupMembers', { conversationId })
  }

  function createGroup(
    name: string,
    avatar: string,
    memberIds: number[],
  ): Promise<{ success: boolean; msg?: string; conversationId?: number }> {
    return new Promise((resolve) => {
      bridge.send('createGroup', { name, avatar, memberIds })
      bridge.on<{ success: boolean; msg?: string; conversationId?: number; groupId?: number }>(
        'createGroupResult',
        (data) => {
          bridge.off('createGroupResult')
          if (data.success) loadGroups()
          resolve(data)
        },
      )
    })
  }

  function leaveGroup(conversationId: number): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('leaveGroup', { conversationId })
      bridge.on<{ success: boolean; msg?: string }>('leaveGroupResult', (data) => {
        bridge.off('leaveGroupResult')
        if (data.success) {
          groups.value = groups.value.filter((g) => g.conversationId !== conversationId)
        }
        resolve(data)
      })
    })
  }

  function dismissGroup(conversationId: number): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('dismissGroup', { conversationId })
      bridge.on<{ success: boolean; msg?: string }>('dismissGroupResult', (data) => {
        bridge.off('dismissGroupResult')
        if (data.success) {
          groups.value = groups.value.filter((g) => g.conversationId !== conversationId)
        }
        resolve(data)
      })
    })
  }

  function kickMember(conversationId: number, targetUid: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('kickMember', { conversationId, targetUid })
      bridge.on<{ success: boolean; msg?: string }>('kickMemberResult', (data) => {
        bridge.off('kickMemberResult')
        if (data.success) getGroupMembers(conversationId)
        resolve(data)
      })
    })
  }

  function updateGroup(conversationId: number, name: string, notice: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('updateGroup', { conversationId, name, notice })
      bridge.on<{ success: boolean; msg?: string }>('updateGroupResult', (data) => {
        bridge.off('updateGroupResult')
        if (data.success) {
          loadGroups()
          getGroupInfo(conversationId)
        }
        resolve(data)
      })
    })
  }

  function setMemberRole(conversationId: number, targetUid: string, role: number): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('setMemberRole', { conversationId, targetUid, role })
      bridge.on<{ success: boolean; msg?: string }>('setMemberRoleResult', (data) => {
        bridge.off('setMemberRoleResult')
        if (data.success) getGroupMembers(conversationId)
        resolve(data)
      })
    })
  }

  return {
    groups,
    currentGroupInfo,
    currentMembers,
    loadingGroups,
    init,
    loadGroups,
    getGroupInfo,
    getGroupMembers,
    createGroup,
    leaveGroup,
    dismissGroup,
    kickMember,
    updateGroup,
    setMemberRole,
  }
})
