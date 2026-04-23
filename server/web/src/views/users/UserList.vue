<template>
  <div class="user-list">
    <!-- 搜索栏 -->
    <el-card shadow="never" class="filter-card">
      <el-form :inline="true" @submit.prevent="handleSearch">
        <el-form-item label="关键词">
          <el-input v-model="query.keyword" placeholder="搜索 UID / 昵称" clearable />
        </el-form-item>
        <el-form-item label="状态">
          <el-select v-model="query.status" placeholder="全部" clearable>
            <el-option label="正常" :value="1" />
            <el-option label="禁用" :value="2" />
            <el-option label="已删除" :value="3" />
          </el-select>
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleSearch">查询</el-button>
          <el-button @click="handleReset">重置</el-button>
          <el-button type="success" @click="showCreate = true">创建用户</el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <!-- 数据表格 -->
    <el-card shadow="never" style="margin-top: 16px">
      <el-table :data="tableData" v-loading="loading" stripe>
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="uid" label="UID" width="180" />
        <el-table-column prop="nickname" label="昵称" />
        <el-table-column label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="statusTag(row.status)">{{ statusText(row.status) }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column label="在线" width="80">
          <template #default="{ row }">
            <el-tag :type="row.is_online ? 'success' : 'info'" size="small">
              {{ row.is_online ? '在线' : '离线' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="created_at" label="创建时间" width="180" />
        <el-table-column label="操作" width="260" fixed="right">
          <template #default="{ row }">
            <el-button size="small" @click="viewDetail(row.uid)">详情</el-button>
            <el-dropdown trigger="click" @command="(cmd: string) => handleAction(cmd, row)">
              <el-button size="small">更多<el-icon class="el-icon--right"><ArrowDown /></el-icon></el-button>
              <template #dropdown>
                <el-dropdown-menu>
                  <el-dropdown-item command="resetPwd">重置密码</el-dropdown-item>
                  <el-dropdown-item command="kick">踢下线</el-dropdown-item>
                  <el-dropdown-item v-if="row.status === 1" command="ban" divided>封禁</el-dropdown-item>
                  <el-dropdown-item v-if="row.status === 2" command="unban">解禁</el-dropdown-item>
                  <el-dropdown-item command="delete" divided style="color: #f56c6c">删除</el-dropdown-item>
                </el-dropdown-menu>
              </template>
            </el-dropdown>
          </template>
        </el-table-column>
      </el-table>

      <el-pagination
        class="pagination"
        v-model:current-page="query.page"
        v-model:page-size="query.page_size"
        :total="total"
        :page-sizes="[20, 50, 100]"
        layout="total, sizes, prev, pager, next"
        @size-change="fetchData"
        @current-change="fetchData"
      />
    </el-card>

    <!-- 创建用户对话框 -->
    <el-dialog v-model="showCreate" title="创建用户" width="460px">
      <el-form ref="createFormRef" :model="createForm" :rules="createRules" label-width="80px">
        <el-form-item label="邮箱" prop="email">
          <el-input v-model="createForm.email" />
        </el-form-item>
        <el-form-item label="密码" prop="password">
          <el-input v-model="createForm.password" type="password" show-password />
        </el-form-item>
        <el-form-item label="昵称" prop="nickname">
          <el-input v-model="createForm.nickname" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="showCreate = false">取消</el-button>
        <el-button type="primary" :loading="creating" @click="handleCreate">确定</el-button>
      </template>
    </el-dialog>

    <!-- 用户详情抽屉 -->
    <el-drawer v-model="showDetail" title="用户详情" size="400px">
      <template v-if="detail">
        <el-descriptions :column="1" border>
          <el-descriptions-item label="ID">{{ detail.id }}</el-descriptions-item>
          <el-descriptions-item label="UID">{{ detail.uid }}</el-descriptions-item>
          <el-descriptions-item label="昵称">{{ detail.nickname }}</el-descriptions-item>
          <el-descriptions-item label="状态">
            <el-tag :type="statusTag(detail.status)">{{ statusText(detail.status) }}</el-tag>
          </el-descriptions-item>
          <el-descriptions-item label="在线">{{ detail.is_online ? '是' : '否' }}</el-descriptions-item>
          <el-descriptions-item label="创建时间">{{ detail.created_at }}</el-descriptions-item>
        </el-descriptions>
        <h4 style="margin: 16px 0 8px">设备列表</h4>
        <el-table :data="detail.devices" size="small" v-if="detail.devices?.length">
          <el-table-column prop="device_name" label="设备" />
          <el-table-column label="状态" width="80">
            <template #default="{ row }">
              <el-tag :type="row.is_online ? 'success' : 'info'" size="small">
                {{ row.is_online ? '在线' : '离线' }}
              </el-tag>
            </template>
          </el-table-column>
          <el-table-column prop="last_seen" label="最后活跃" />
        </el-table>
        <el-empty v-else description="暂无设备" />
      </template>
    </el-drawer>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { getUsers, getUserDetail, createUser, deleteUser, resetPassword, banUser, unbanUser, kickUser } from '@/api/user'
import type { User, UserDetail } from '@/api/user'
import { ElMessage, ElMessageBox } from 'element-plus'
import type { FormInstance, FormRules } from 'element-plus'

const loading = ref(false)
const tableData = ref<User[]>([])
const total = ref(0)
const query = reactive({ page: 1, page_size: 20, keyword: '', status: undefined as number | undefined })

// 创建
const showCreate = ref(false)
const creating = ref(false)
const createFormRef = ref<FormInstance>()
const createForm = reactive({ email: '', password: '', nickname: '' })

// 密码强度：>=8 且至少 3 类（大写/小写/数字/特殊）
function validateStrongPassword(_: unknown, value: string, cb: (err?: Error) => void) {
  if (!value) return cb(new Error('请输入密码'))
  if (value.length < 8) return cb(new Error('密码至少 8 位'))
  if (value.length > 128) return cb(new Error('密码最多 128 位'))
  let classes = 0
  if (/[a-z]/.test(value)) classes++
  if (/[A-Z]/.test(value)) classes++
  if (/\d/.test(value)) classes++
  if (/[^A-Za-z0-9]/.test(value)) classes++
  if (classes < 3) return cb(new Error('需包含大写/小写/数字/特殊字符中任意 3 类'))
  cb()
}

const createRules: FormRules = {
  email: [{ required: true, message: '请输入邮箱', trigger: 'blur' }],
  password: [
    { required: true, validator: validateStrongPassword, trigger: 'blur' },
  ],
}

// 按行 loading：防重复提交
const rowLoading = reactive<Record<string, boolean>>({})

// 详情
const showDetail = ref(false)
const detail = ref<UserDetail | null>(null)

function statusText(s: number) {
  return s === 1 ? '正常' : s === 2 ? '禁用' : '已删除'
}
function statusTag(s: number) {
  return (s === 1 ? 'success' : s === 2 ? 'danger' : 'info') as 'success' | 'danger' | 'info'
}

async function fetchData() {
  loading.value = true
  try {
    const res = await getUsers(query)
    if (res.data.code === 0) {
      tableData.value = res.data.data.items
      total.value = res.data.data.total
    }
  } finally {
    loading.value = false
  }
}

function handleSearch() {
  query.page = 1
  fetchData()
}

function handleReset() {
  query.keyword = ''
  query.status = undefined
  query.page = 1
  fetchData()
}

async function handleCreate() {
  const valid = await createFormRef.value?.validate().catch(() => false)
  if (!valid) return
  creating.value = true
  try {
    const res = await createUser(createForm)
    if (res.data.code === 0) {
      ElMessage.success('创建成功')
      showCreate.value = false
      createForm.email = ''
      createForm.password = ''
      createForm.nickname = ''
      fetchData()
    } else {
      ElMessage.error(res.data.msg)
    }
  } finally {
    creating.value = false
  }
}

async function viewDetail(uid: string) {
  const res = await getUserDetail(uid)
  if (res.data.code === 0) {
    detail.value = res.data.data
    showDetail.value = true
  }
}

async function handleAction(cmd: string, row: User) {
  if (rowLoading[row.uid]) return
  if (cmd === 'resetPwd') {
    const { value } = await ElMessageBox.prompt('请输入新密码（至少 8 位，含大/小写/数字/特殊中任意 3 类）', '重置密码', {
      inputValidator: (v: string) => {
        if (!v || v.length < 8) return '密码至少 8 位'
        let c = 0
        if (/[a-z]/.test(v)) c++
        if (/[A-Z]/.test(v)) c++
        if (/\d/.test(v)) c++
        if (/[^A-Za-z0-9]/.test(v)) c++
        return c >= 3 ? true : '需包含大写/小写/数字/特殊字符中任意 3 类'
      },
    })
    rowLoading[row.uid] = true
    try {
      const res = await resetPassword(row.uid, value)
      if (res.data.code === 0) ElMessage.success('密码已重置')
      else ElMessage.error(res.data.msg)
    } finally { rowLoading[row.uid] = false }
  } else if (cmd === 'kick') {
    rowLoading[row.uid] = true
    try {
      const res = await kickUser(row.uid)
      if (res.data.code === 0) {
        ElMessage.success(`已踢出 ${res.data.data.kicked_devices} 个设备`)
        fetchData()
      }
    } finally { rowLoading[row.uid] = false }
  } else if (cmd === 'ban') {
    const { value } = await ElMessageBox.prompt('请输入封禁原因', '封禁用户')
    rowLoading[row.uid] = true
    try {
      const res = await banUser(row.uid, value)
      if (res.data.code === 0) {
        ElMessage.success('已封禁')
        fetchData()
      }
    } finally { rowLoading[row.uid] = false }
  } else if (cmd === 'unban') {
    rowLoading[row.uid] = true
    try {
      const res = await unbanUser(row.uid)
      if (res.data.code === 0) {
        ElMessage.success('已解禁')
        fetchData()
      }
    } finally { rowLoading[row.uid] = false }
  } else if (cmd === 'delete') {
    await ElMessageBox.confirm('确定删除该用户？此操作不可恢复。', '删除用户', { type: 'warning' })
    rowLoading[row.uid] = true
    try {
      const res = await deleteUser(row.uid)
      if (res.data.code === 0) {
        ElMessage.success('已删除')
        fetchData()
      }
    } finally { rowLoading[row.uid] = false }
  }
}

onMounted(fetchData)
</script>

<style scoped lang="scss">
.filter-card {
  :deep(.el-card__body) {
    padding-bottom: 0;
  }
}
.pagination {
  margin-top: 16px;
  justify-content: flex-end;
}
</style>
