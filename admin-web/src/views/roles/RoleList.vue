<template>
  <div class="role-list">
    <el-card shadow="never">
      <template #header>
        <div style="display: flex; justify-content: space-between; align-items: center">
          <span>角色列表</span>
          <el-button type="success" @click="showCreate = true">创建角色</el-button>
        </div>
      </template>
      <el-table :data="roles" v-loading="loading" stripe>
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="name" label="角色名" width="180" />
        <el-table-column prop="description" label="描述" />
        <el-table-column label="权限">
          <template #default="{ row }">
            <el-tag v-for="p in row.permissions" :key="p" size="small" style="margin: 2px">{{ p }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="160" fixed="right">
          <template #default="{ row }">
            <el-button size="small" @click="startEdit(row)">编辑</el-button>
            <el-button size="small" type="danger" @click="handleDelete(row)">删除</el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <!-- 创建/编辑角色对话框 -->
    <el-dialog v-model="showForm" :title="editing ? '编辑角色' : '创建角色'" width="560px">
      <el-form :model="form" label-width="80px">
        <el-form-item label="角色名" v-if="!editing">
          <el-input v-model="form.name" />
        </el-form-item>
        <el-form-item label="描述">
          <el-input v-model="form.description" />
        </el-form-item>
        <el-form-item label="权限">
          <el-checkbox-group v-model="form.permissions">
            <el-checkbox v-for="p in ALL_PERMISSIONS" :key="p" :label="p" :value="p">
              {{ p }}
            </el-checkbox>
          </el-checkbox-group>
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="showForm = false">取消</el-button>
        <el-button type="primary" :loading="saving" @click="handleSave">确定</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { getRoles, createRole, updateRole, deleteRole, ALL_PERMISSIONS } from '@/api/admin'
import type { Role } from '@/api/admin'
import { ElMessage, ElMessageBox } from 'element-plus'

const loading = ref(false)
const roles = ref<Role[]>([])

const showForm = ref(false)
const showCreate = ref(false)
const editing = ref<Role | null>(null)
const saving = ref(false)
const form = reactive({ name: '', description: '', permissions: [] as string[] })

async function fetchData() {
  loading.value = true
  try {
    const res = await getRoles()
    if (res.data.code === 0) {
      roles.value = res.data.data
    }
  } finally {
    loading.value = false
  }
}

function startEdit(row: Role) {
  editing.value = row
  form.name = row.name
  form.description = row.description
  form.permissions = [...row.permissions]
  showForm.value = true
}

// watch showCreate → open form in create mode
import { watch } from 'vue'
watch(showCreate, (v) => {
  if (v) {
    editing.value = null
    form.name = ''
    form.description = ''
    form.permissions = []
    showForm.value = true
  }
})

async function handleSave() {
  saving.value = true
  try {
    if (editing.value) {
      const res = await updateRole(editing.value.id, { description: form.description, permissions: form.permissions })
      if (res.data.code === 0) ElMessage.success('已更新')
      else ElMessage.error(res.data.msg)
    } else {
      if (!form.name) { ElMessage.warning('请输入角色名'); return }
      const res = await createRole({ name: form.name, description: form.description, permissions: form.permissions })
      if (res.data.code === 0) ElMessage.success('已创建')
      else ElMessage.error(res.data.msg)
    }
    showForm.value = false
    showCreate.value = false
    fetchData()
  } finally {
    saving.value = false
  }
}

async function handleDelete(row: Role) {
  await ElMessageBox.confirm(`确定删除角色 "${row.name}"？`, '删除确认', { type: 'warning' })
  const res = await deleteRole(row.id)
  if (res.data.code === 0) {
    ElMessage.success('已删除')
    fetchData()
  }
}

onMounted(fetchData)
</script>
