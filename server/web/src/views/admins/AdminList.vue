<template>
  <div class="admin-list">
    <el-card shadow="never" class="filter-card">
      <el-form :inline="true" @submit.prevent="handleSearch">
        <el-form-item label="关键词">
          <el-input v-model="query.keyword" placeholder="搜索账号 / 昵称" clearable />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleSearch">查询</el-button>
          <el-button @click="handleReset">重置</el-button>
          <el-button type="success" @click="showCreate = true">添加管理员</el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <el-card shadow="never" style="margin-top: 16px">
      <el-table :data="tableData" v-loading="loading" stripe>
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="uid" label="账号" width="160" />
        <el-table-column prop="nickname" label="昵称" width="140" />
        <el-table-column label="角色">
          <template #default="{ row }">
            <el-tag v-for="r in row.roles" :key="r" size="small" style="margin: 2px">{{ r }}</el-tag>
            <span v-if="!row.roles?.length" style="color: #909399">无角色</span>
          </template>
        </el-table-column>
        <el-table-column label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.status === 1 ? 'success' : 'danger'">
              {{ row.status === 1 ? '正常' : '禁用' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="created_at" label="创建时间" width="180" />
        <el-table-column label="操作" width="200" fixed="right">
          <template #default="{ row }">
            <el-button size="small" @click="openRoleDialog(row)">角色</el-button>
            <el-dropdown trigger="click" @command="(cmd: string) => handleAction(cmd, row)">
              <el-button size="small">更多<el-icon class="el-icon--right"><ArrowDown /></el-icon></el-button>
              <template #dropdown>
                <el-dropdown-menu>
                  <el-dropdown-item command="resetPwd">重置密码</el-dropdown-item>
                  <el-dropdown-item v-if="row.status === 2" command="enable">启用</el-dropdown-item>
                  <el-dropdown-item v-if="row.status === 1 && row.id !== 1" command="disable">禁用</el-dropdown-item>
                  <el-dropdown-item command="delete" divided :disabled="row.id === 1" style="color: #f56c6c">删除</el-dropdown-item>
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

    <!-- 创建管理员对话框 -->
    <el-dialog v-model="showCreate" title="添加管理员" width="460px">
      <el-form ref="formRef" :model="createForm" :rules="rules" label-width="80px">
        <el-form-item label="账号" prop="uid">
          <el-input v-model="createForm.uid" />
        </el-form-item>
        <el-form-item label="密码" prop="password">
          <el-input v-model="createForm.password" type="password" show-password />
        </el-form-item>
        <el-form-item label="昵称">
          <el-input v-model="createForm.nickname" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="showCreate = false">取消</el-button>
        <el-button type="primary" :loading="creating" @click="handleCreate">确定</el-button>
      </template>
    </el-dialog>

    <!-- 角色分配对话框 -->
    <el-dialog v-model="showRoleDialog" title="分配角色" width="460px">
      <p style="margin-bottom: 12px; color: #606266">管理员: <strong>{{ roleTarget?.uid }}</strong></p>
      <el-checkbox-group v-model="selectedRoleIds">
        <el-checkbox v-for="r in allRoles" :key="r.id" :label="r.id" :value="r.id" style="display: block; margin-bottom: 8px">
          {{ r.name }}
          <span v-if="r.description" style="color: #909399; font-size: 12px; margin-left: 8px">{{ r.description }}</span>
        </el-checkbox>
      </el-checkbox-group>
      <template #footer>
        <el-button @click="showRoleDialog = false">取消</el-button>
        <el-button type="primary" :loading="savingRoles" @click="handleSaveRoles">确定</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import {
  getAdmins, createAdmin, deleteAdmin,
  resetAdminPassword, enableAdmin, disableAdmin,
  getRoles, setAdminRoles,
} from '@/api/admin'
import type { Admin, Role } from '@/api/admin'
import { ElMessage, ElMessageBox } from 'element-plus'
import type { FormInstance, FormRules } from 'element-plus'

const loading = ref(false)
const tableData = ref<Admin[]>([])
const total = ref(0)
const query = reactive({ page: 1, page_size: 20, keyword: '' })

// 创建
const showCreate = ref(false)
const creating = ref(false)
const formRef = ref<FormInstance>()
const createForm = reactive({ uid: '', password: '', nickname: '' })

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

const rules: FormRules = {
  uid: [{ required: true, message: '请输入账号', trigger: 'blur' }],
  password: [
    { required: true, validator: validateStrongPassword, trigger: 'blur' },
  ],
}

// 按行操作 loading：防止重复点击触发多次删除/禁用
const rowLoading = reactive<Record<number, boolean>>({})

// 角色分配
const showRoleDialog = ref(false)
const savingRoles = ref(false)
const roleTarget = ref<Admin | null>(null)
const selectedRoleIds = ref<number[]>([])
const allRoles = ref<Role[]>([])

async function fetchData() {
  loading.value = true
  try {
    const res = await getAdmins(query)
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
  query.page = 1
  fetchData()
}

async function handleCreate() {
  const valid = await formRef.value?.validate().catch(() => false)
  if (!valid) return
  creating.value = true
  try {
    const res = await createAdmin(createForm)
    if (res.data.code === 0) {
      ElMessage.success('创建成功')
      showCreate.value = false
      createForm.uid = ''
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

async function openRoleDialog(row: Admin) {
  roleTarget.value = row
  // 加载所有角色
  const res = await getRoles()
  if (res.data.code === 0) {
    allRoles.value = res.data.data
    // 根据 row.roles (name[]) 匹配 role id
    selectedRoleIds.value = allRoles.value
      .filter(r => row.roles?.includes(r.name))
      .map(r => r.id)
    showRoleDialog.value = true
  }
}

async function handleSaveRoles() {
  if (!roleTarget.value) return
  savingRoles.value = true
  try {
    const res = await setAdminRoles(roleTarget.value.id, selectedRoleIds.value)
    if (res.data.code === 0) {
      ElMessage.success('角色已更新')
      showRoleDialog.value = false
      fetchData()
    } else {
      ElMessage.error(res.data.msg)
    }
  } finally {
    savingRoles.value = false
  }
}

async function handleAction(cmd: string, row: Admin) {
  if (rowLoading[row.id]) return  // 防抖：该行已有操作在执行
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
    rowLoading[row.id] = true
    try {
      const res = await resetAdminPassword(row.id, value)
      if (res.data.code === 0) ElMessage.success('密码已重置')
      else ElMessage.error(res.data.msg)
    } finally { rowLoading[row.id] = false }
  } else if (cmd === 'enable') {
    rowLoading[row.id] = true
    try {
      const res = await enableAdmin(row.id)
      if (res.data.code === 0) {
        ElMessage.success('已启用')
        fetchData()
      } else {
        ElMessage.error(res.data.msg)
      }
    } finally { rowLoading[row.id] = false }
  } else if (cmd === 'disable') {
    await ElMessageBox.confirm(`确定禁用管理员 "${row.uid}"？禁用后该管理员将无法登录。`, '禁用确认', { type: 'warning' })
    rowLoading[row.id] = true
    try {
      const res = await disableAdmin(row.id)
      if (res.data.code === 0) {
        ElMessage.success('已禁用')
        fetchData()
      } else {
        ElMessage.error(res.data.msg)
      }
    } finally { rowLoading[row.id] = false }
  } else if (cmd === 'delete') {
    await ElMessageBox.confirm(`确定删除管理员 "${row.uid}"？此操作不可恢复。`, '删除确认', { type: 'warning' })
    rowLoading[row.id] = true
    try {
      const res = await deleteAdmin(row.id)
      if (res.data.code === 0) {
        ElMessage.success('已删除')
        fetchData()
      } else {
        ElMessage.error(res.data.msg)
      }
    } finally { rowLoading[row.id] = false }
  }
}

onMounted(fetchData)
</script>

<style scoped lang="scss">
.filter-card {
  :deep(.el-card__body) { padding-bottom: 0; }
}
.pagination {
  margin-top: 16px;
  justify-content: flex-end;
}
</style>
