<script setup lang="ts">
import { ref, computed } from 'vue'

export interface ToastItem {
  id: number
  message: string
  type: 'success' | 'error' | 'warning' | 'info'
}

const toasts = ref<ToastItem[]>([])
let nextId = 1

function show(message: string, type: ToastItem['type'] = 'info', duration = 3000) {
  const id = nextId++
  toasts.value.push({ id, message, type })
  setTimeout(() => {
    toasts.value = toasts.value.filter((t) => t.id !== id)
  }, duration)
}

const visibleToasts = computed(() => toasts.value.slice(-5))

defineExpose({ show })
</script>

<template>
  <Teleport to="body">
    <div class="toast-container">
      <Transition v-for="toast in visibleToasts" :key="toast.id" name="toast" appear>
        <div class="toast-item" :class="'toast-' + toast.type">
          <span class="toast-icon">
            {{ toast.type === 'success' ? '✓' : toast.type === 'error' ? '✕' : toast.type === 'warning' ? '⚠' : 'ℹ' }}
          </span>
          <span class="toast-msg">{{ toast.message }}</span>
        </div>
      </Transition>
    </div>
  </Teleport>
</template>

<style scoped>
.toast-container {
  position: fixed;
  top: 24px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 9999;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  pointer-events: none;
}

.toast-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 20px;
  border-radius: 6px;
  font-size: 14px;
  background: var(--bg-primary, #fff);
  border: 1px solid var(--border-light, #e4e7ed);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
  pointer-events: auto;
  min-width: 200px;
  max-width: 400px;
}

.toast-icon {
  font-size: 16px;
  flex-shrink: 0;
}

.toast-success { border-left: 3px solid var(--success, #67c23a); }
.toast-success .toast-icon { color: var(--success, #67c23a); }
.toast-error { border-left: 3px solid var(--danger, #f56c6c); }
.toast-error .toast-icon { color: var(--danger, #f56c6c); }
.toast-warning { border-left: 3px solid var(--warning, #e6a23c); }
.toast-warning .toast-icon { color: var(--warning, #e6a23c); }
.toast-info { border-left: 3px solid var(--primary, #409eff); }
.toast-info .toast-icon { color: var(--primary, #409eff); }

.toast-enter-active { transition: all 0.3s ease; }
.toast-leave-active { transition: all 0.3s ease; }
.toast-enter-from { opacity: 0; transform: translateY(-12px); }
.toast-leave-to { opacity: 0; transform: translateY(-12px); }
</style>
