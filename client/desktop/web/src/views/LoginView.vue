<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const auth = useAuthStore()

// ---- 账号缓存 ----
const MAX_SAVED = 5
const STORAGE_KEY = 'nova_saved_accounts'

interface SavedAccount {
  email: string
  nickname: string
  avatar: string
}

const savedAccounts = ref<SavedAccount[]>([])
const showDropdown = ref(false)

function loadSavedAccounts() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    savedAccounts.value = raw ? JSON.parse(raw) : []
  } catch {
    savedAccounts.value = []
  }
}

function saveAccount(email: string, nickname: string, avatar: string) {
  const list = savedAccounts.value.filter((a) => a.email !== email)
  list.unshift({ email, nickname, avatar })
  if (list.length > MAX_SAVED) list.length = MAX_SAVED
  savedAccounts.value = list
  localStorage.setItem(STORAGE_KEY, JSON.stringify(list))
}

function removeAccount(email: string, e: Event) {
  e.stopPropagation()
  savedAccounts.value = savedAccounts.value.filter((a) => a.email !== email)
  localStorage.setItem(STORAGE_KEY, JSON.stringify(savedAccounts.value))
}

function selectAccount(account: SavedAccount) {
  loginEmail.value = account.email
  showDropdown.value = false
}

function onEmailFocus() {
  if (savedAccounts.value.length > 0) showDropdown.value = true
}

function onEmailBlur() {
  // 延迟关闭，让 click 事件先触发
  setTimeout(() => (showDropdown.value = false), 200)
}

onMounted(loadSavedAccounts)

// ---- 表单模式 ----
const isLogin = ref(true)

// ---- 登录字段 ----
const loginEmail = ref('')
const loginPassword = ref('')
const loginError = ref('')
const loginErrorStyle = ref('')
const loginLoading = ref(false)

const loginDisabled = computed(
  () => !loginEmail.value.trim() || !loginPassword.value || loginLoading.value,
)

// ---- 注册字段 ----
const regEmail = ref('')
const regNickname = ref('')
const regPassword = ref('')
const regPassword2 = ref('')
const regError = ref('')
const regLoading = ref(false)

const regDisabled = computed(
  () =>
    !regEmail.value.trim() ||
    !regNickname.value.trim() ||
    !regPassword.value ||
    !regPassword2.value ||
    regLoading.value,
)

// ---- 登录 ----
async function handleLogin() {
  loginError.value = ''
  loginErrorStyle.value = ''
  loginLoading.value = true

  const result = await auth.login(loginEmail.value.trim(), loginPassword.value)
  if (result.success) {
    saveAccount(loginEmail.value.trim(), result.nickname || '', result.avatar || '')
    router.push('/main')
  } else {
    loginError.value = result.msg || '登录失败'
    loginLoading.value = false
  }
}

// ---- 注册 ----
async function handleRegister() {
  regError.value = ''

  if (regPassword.value !== regPassword2.value) {
    regError.value = '两次密码输入不一致'
    return
  }
  if (regPassword.value.length < 6) {
    regError.value = '密码长度至少6位'
    return
  }

  regLoading.value = true
  const result = await auth.register(
    regEmail.value.trim(),
    regNickname.value.trim(),
    regPassword.value,
  )

  if (result.success) {
    isLogin.value = true
    loginEmail.value = regEmail.value
    loginPassword.value = ''
    loginError.value = '注册成功，请登录'
    loginErrorStyle.value = 'color: var(--success)'
  } else {
    regError.value = result.msg || '注册失败'
  }
  regLoading.value = false
}

function onLoginKeydown(e: KeyboardEvent) {
  if (e.key === 'Enter' && !loginDisabled.value) handleLogin()
}

function onRegKeydown(e: KeyboardEvent) {
  if (e.key === 'Enter' && !regDisabled.value) handleRegister()
}
</script>

<template>
  <div class="login-page">
    <div class="login-card">
      <h1>NovaIIM</h1>
      <p class="subtitle">即时通讯客户端</p>

      <!-- 登录表单 -->
      <div v-if="isLogin">
        <div class="form-group account-input">
          <input
            v-model="loginEmail"
            type="email"
            placeholder="邮箱地址"
            autocomplete="off"
            @keydown="onLoginKeydown"
            @focus="onEmailFocus"
            @blur="onEmailBlur"
          />
          <div v-if="showDropdown && savedAccounts.length" class="account-dropdown">
            <div
              v-for="acc in savedAccounts"
              :key="acc.email"
              class="account-item"
              @mousedown.prevent="selectAccount(acc)"
            >
              <div class="account-info">
                <span class="account-email">{{ acc.email }}</span>
              </div>
              <span class="account-remove" @mousedown.prevent.stop="removeAccount(acc.email, $event)">×</span>
            </div>
          </div>
        </div>
        <div class="form-group">
          <input
            v-model="loginPassword"
            type="password"
            placeholder="密码"
            @keydown="onLoginKeydown"
          />
        </div>
        <div class="login-error" :style="loginErrorStyle">{{ loginError }}</div>
        <button class="btn-primary" :disabled="loginDisabled" @click="handleLogin">
          {{ loginLoading ? '登录中...' : '登 录' }}
        </button>
        <p class="switch-link">
          没有账号？<a href="#" @click.prevent="isLogin = false">立即注册</a>
        </p>
      </div>

      <!-- 注册表单 -->
      <div v-else>
        <div class="form-group">
          <input v-model="regEmail" type="email" placeholder="邮箱地址" />
        </div>
        <div class="form-group">
          <input v-model="regNickname" type="text" placeholder="昵称" />
        </div>
        <div class="form-group">
          <input v-model="regPassword" type="password" placeholder="密码（至少6位）" />
        </div>
        <div class="form-group">
          <input
            v-model="regPassword2"
            type="password"
            placeholder="确认密码"
            @keydown="onRegKeydown"
          />
        </div>
        <div class="login-error">{{ regError }}</div>
        <button class="btn-primary" :disabled="regDisabled" @click="handleRegister">
          {{ regLoading ? '注册中...' : '注 册' }}
        </button>
        <p class="switch-link">
          已有账号？<a href="#" @click.prevent="isLogin = true">返回登录</a>
        </p>
      </div>
    </div>
  </div>
</template>

<style scoped>
.account-input {
  position: relative;
}

.account-dropdown {
  position: absolute;
  top: 100%;
  left: 0;
  right: 0;
  background: var(--bg-card, #2a2a2a);
  border: 1px solid var(--border, #404040);
  border-radius: 6px;
  margin-top: 4px;
  max-height: 200px;
  overflow-y: auto;
  z-index: 10;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}

.account-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 12px;
  cursor: pointer;
  transition: background 0.15s;
}

.account-item:hover {
  background: var(--bg-hover, #333);
}

.account-info {
  flex: 1;
  min-width: 0;
}

.account-email {
  font-size: 13px;
  color: var(--text-primary, #e0e0e0);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  line-height: 20px;
}

.account-remove {
  flex-shrink: 0;
  width: 20px;
  height: 20px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  color: var(--text-secondary, #888);
  border-radius: 50%;
  cursor: pointer;
  transition: all 0.15s;
}

.account-remove:hover {
  color: var(--danger, #f56c6c);
  background: rgba(245, 108, 108, 0.1);
}
</style>
