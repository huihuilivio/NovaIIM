<template>
  <div class="admin-list">
    <el-card shadow="never" class="filter-card">
      <el-form :inline="true" @submit.prevent="handleSearch">
        <el-form-item label="关键词">
          <el-input v-model="query.keyword" placeholder="搜索账号 / 昵称" clearable />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleSearch">查询</el-button>
          <el-button type="success" @click="showCreate = true">添加管理员</el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <el-card shadow="never" style="margin-top: 16px">
      <el-table :data="tableData" v-loading="loading" stripe>
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="uid" label="账号" width="160" />
        <el-table-column prop="nickname" label="昵称" />
        <el-table-column label="角色">
          <template #default="{ row }">
            <el-tag v-for="r in row.roles" :key="r" size="small" style="margin-right: 4px">{{ r }}</el-tag>
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
        <el-table-column label="操作" width="120" fixed="right">
          <template #default="{ row }">
            <el-button size="small" type="danger" @click="handleDelete(row)" :disabled="row.id === 1">
              删除
            </el-button>
          </template>
        </el-table-column>
      </el-table>

      <el-pagination
        class="pagination"
        v-model:current-page="query.page"
        v-model:page-size="query.page_size"
        :total="total"
        layout="total, prev, pager, next"
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
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { getAdmins, createAdmin, deleteAdmin } from '@/api/admin'
import type { Admin } from '@/api/admin'
import { ElMessage, ElMessageBox } from 'element-plus'
import type { FormInstance, FormRules } from 'element-plus'

const loading = ref(false)
const tableData = ref<Admin[]>([])
const total = ref(0)
const query = reactive({ page: 1, page_size: 20, keyword: '' })

const showCreate = ref(false)
const creating = ref(false)
const formRef = ref<FormInstance>()
const createForm = reactive({ uid: '', password: '', nickname: '' })
const rules: FormRules = {
  uid: [{ required: true, message: '请输入账号', trigger: 'blur' }],
  password: [
    { required: true, message: '请输入密码', trigger: 'blur' },
    { min: 6, message: '密码至少 6 位', trigger: 'blur' },
  ],
}

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

async function handleDelete(row: Admin) {
  await ElMessageBox.confirm(`确定删除管理员 "${row.uid}"？`, '删除确认', { type: 'warning' })
  const res = await deleteAdmin(row.id)
  if (res.data.code === 0) {
    ElMessage.success('已删除')
    fetchData()
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
