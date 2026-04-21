import { defineStore } from 'pinia'
import { ref } from 'vue'
import { bridge } from '@/bridge'

export type ConnectionState =
  | 'Disconnected'
  | 'Connecting'
  | 'Connected'
  | 'Authenticated'
  | 'Reconnecting'

const stateLabels: Record<string, string> = {
  Disconnected: '未连接',
  Connecting: '连接中...',
  Connected: '已连接',
  Authenticated: '已认证',
  Reconnecting: '重连中...',
}

export const useConnectionStore = defineStore('connection', () => {
  const state = ref<ConnectionState>('Disconnected')
  const label = ref('未连接')

  function init() {
    bridge.on<{ state: ConnectionState }>('connectionState', (data) => {
      state.value = data.state
      label.value = stateLabels[data.state] ?? data.state
    })
  }

  function connect() {
    bridge.send('connect')
  }

  function disconnect() {
    bridge.send('disconnect')
  }

  return { state, label, init, connect, disconnect }
})
