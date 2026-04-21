import { defineStore } from 'pinia'
import { ref } from 'vue'
import { bridge } from '@/bridge'

export interface Friend {
  uid: string
  nickname: string
  avatar: string
  conversationId: number
}

export interface FriendRequest {
  requestId: number
  fromUid: string
  fromNickname: string
  fromAvatar: string
  remark: string
  createdAt: string
  status: number // 0=pending, 1=accepted, 2=rejected
}

export interface SearchUserEntry {
  uid: string
  nickname: string
  avatar: string
}

export interface UserProfile {
  uid: string
  nickname: string
  avatar: string
  email: string
  status: number
  createdAt: string
}

export interface FriendNotification {
  notifyType: number // 1=新申请 2=已同意 3=已拒绝 4=已删除
  fromUid: string
  fromNickname: string
  fromAvatar: string
  remark: string
  requestId: number
  conversationId: number
}

export const useContactsStore = defineStore('contacts', () => {
  const friends = ref<Friend[]>([])
  const friendRequests = ref<FriendRequest[]>([])
  const searchResults = ref<SearchUserEntry[]>([])
  const pendingCount = ref(0)
  const selectedProfile = ref<UserProfile | null>(null)
  const loadingFriends = ref(false)
  let initialized = false

  function init() {
    if (initialized) return
    initialized = true
    bridge.on<{ success: boolean; friends: Friend[] }>('friendList', (data) => {
      loadingFriends.value = false
      if (data.success) {
        friends.value = data.friends
      }
    })

    bridge.on<{ success: boolean; requests: FriendRequest[]; total: number }>(
      'friendRequests',
      (data) => {
        if (data.success) {
          friendRequests.value = data.requests
          pendingCount.value = data.requests.filter((r) => r.status === 0).length
        }
      },
    )

    bridge.on<{ success: boolean; users: SearchUserEntry[] }>('searchUserResult', (data) => {
      if (data.success) {
        searchResults.value = data.users
      }
    })

    bridge.on<UserProfile & { success: boolean }>('userProfile', (data) => {
      if (data.success) {
        selectedProfile.value = data
      }
    })

    bridge.on<FriendNotification>('friendNotify', (data) => {
      if (data.notifyType === 1) {
        // 新好友申请 — 刷新请求列表
        pendingCount.value++
        loadFriendRequests()
      } else if (data.notifyType === 2) {
        // 对方同意 — 刷新好友列表
        loadFriendList()
      } else if (data.notifyType === 4) {
        // 被删除 — 移除好友
        friends.value = friends.value.filter((f) => f.uid !== data.fromUid)
      }
    })
  }

  function loadFriendList() {
    loadingFriends.value = true
    bridge.send('getFriendList')
  }

  function loadFriendRequests() {
    bridge.send('getFriendRequests', { page: 1, pageSize: 50 })
  }

  function addFriend(targetUid: string, remark: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('addFriend', { targetUid, remark })
      bridge.on<{ success: boolean; msg?: string }>('addFriendResult', (data) => {
        bridge.off('addFriendResult')
        resolve(data)
      })
    })
  }

  function handleRequest(requestId: number, action: number): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('handleFriendRequest', { requestId, action_type: action })
      bridge.on<{ success: boolean; msg?: string; conversationId?: number }>(
        'handleFriendRequestResult',
        (data) => {
          bridge.off('handleFriendRequestResult')
          if (data.success) {
            // 更新本地状态
            const req = friendRequests.value.find((r) => r.requestId === requestId)
            if (req) req.status = action
            if (action === 1) loadFriendList() // 接受后刷新好友列表
          }
          resolve(data)
        },
      )
    })
  }

  function deleteFriend(targetUid: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('deleteFriend', { targetUid })
      bridge.on<{ success: boolean; msg?: string }>('deleteFriendResult', (data) => {
        bridge.off('deleteFriendResult')
        if (data.success) {
          friends.value = friends.value.filter((f) => f.uid !== targetUid)
        }
        resolve(data)
      })
    })
  }

  function searchUser(keyword: string) {
    if (!keyword.trim()) {
      searchResults.value = []
      return
    }
    bridge.send('searchUser', { keyword })
  }

  function getUserProfile(uid: string) {
    bridge.send('getUserProfile', { targetUid: uid })
  }

  function blockFriend(targetUid: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('blockFriend', { targetUid })
      bridge.on<{ success: boolean; msg?: string }>('blockFriendResult', (data) => {
        bridge.off('blockFriendResult')
        resolve(data)
      })
    })
  }

  function unblockFriend(targetUid: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('unblockFriend', { targetUid })
      bridge.on<{ success: boolean; msg?: string }>('unblockFriendResult', (data) => {
        bridge.off('unblockFriendResult')
        resolve(data)
      })
    })
  }

  function updateProfile(nickname: string, avatar: string): Promise<{ success: boolean; msg?: string }> {
    return new Promise((resolve) => {
      bridge.send('updateProfile', { nickname, avatar })
      bridge.on<{ success: boolean; msg?: string }>('updateProfileResult', (data) => {
        bridge.off('updateProfileResult')
        resolve(data)
      })
    })
  }

  return {
    friends,
    friendRequests,
    searchResults,
    pendingCount,
    selectedProfile,
    loadingFriends,
    init,
    loadFriendList,
    loadFriendRequests,
    addFriend,
    handleRequest,
    deleteFriend,
    searchUser,
    getUserProfile,
    blockFriend,
    unblockFriend,
    updateProfile,
  }
})
