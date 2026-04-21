<template>
  <div class="audit-list">
    <el-card shadow="never" class="filter-card">
      <el-form :inline="true" @submit.prevent="handleSearch">
        <el-form-item label="操作者">
          <el-input v-model.number="query.admin_id" placeholder="管理员 ID" clearable />
        </el-form-item>
        <el-form-item label="操作类型">
          <el-select v-model="query.action" placeholder="全部" clearable>
            <el-option label="user.create" value="user.create" />
            <el-option label="user.delete" value="user.delete" />
            <el-option label="user.ban" value="user.ban" />
            <el-option label="user.unban" value="user.unban" />
            <el-option label="user.kick" value="user.kick" />
            <el-option label="user.reset_password" value="user.reset_password" />
            <el-option label="msg.recall" value="msg.recall" />
            <el-option label="admin.login" value="admin.login" />
            <el-option label="admin.logout" value="admin.logout" />
            <el-option label="admin.create" value="admin.create" />
            <el-option label="admin.delete" value="admin.delete" />
            <el-option label="admin.reset_password" value="admin.reset_password" />
            <el-option label="admin.enable" value="admin.enable" />
            <el-option label="admin.disable" value="admin.disable" />
            <el-option label="admin.set_roles" value="admin.set_roles" />
            <el-option label="role.create" value="role.create" />
            <el-option label="role.update" value="role.update" />
            <el-option label="role.delete" value="role.delete" />
          </el-select>
        </el-form-item>
        <el-form-item label="时间范围">
          <el-date-picker
            v-model="dateRange"
            type="datetimerange"
            range-separator="至"
            start-placeholder="开始"
            end-placeholder="结束"
            format="YYYY-MM-DD HH:mm"
            value-format="YYYY-MM-DDTHH:mm:ssZ"
          />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" @click="handleSearch">查询</el-button>
          <el-button @click="handleReset">重置</el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <el-card shadow="never" style="margin-top: 16px">
      <el-table :data="tableData" v-loading="loading" stripe>
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="operator_uid" label="操作者" width="120" />
        <el-table-column prop="action" label="操作" width="160" />
        <el-table-column prop="target_type" label="目标类型" width="100" />
        <el-table-column prop="target_id" label="目标ID" width="100" />
        <el-table-column prop="detail" label="详情" show-overflow-tooltip>
          <template #default="{ row }">
            {{ typeof row.detail === 'object' ? JSON.stringify(row.detail) : row.detail }}
          </template>
        </el-table-column>
        <el-table-column prop="ip" label="IP" width="140" />
        <el-table-column prop="created_at" label="时间" width="180" />
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
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { getAuditLogs } from '@/api/audit'
import type { AuditLog } from '@/api/audit'

const loading = ref(false)
const tableData = ref<AuditLog[]>([])
const total = ref(0)
const dateRange = ref<[string, string] | null>(null)
const query = reactive({
  page: 1,
  page_size: 20,
  admin_id: undefined as number | undefined,
  action: undefined as string | undefined,
  start_time: undefined as string | undefined,
  end_time: undefined as string | undefined,
})

async function fetchData() {
  if (dateRange.value) {
    query.start_time = dateRange.value[0]
    query.end_time = dateRange.value[1]
  } else {
    query.start_time = undefined
    query.end_time = undefined
  }
  loading.value = true
  try {
    const res = await getAuditLogs(query)
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
  query.admin_id = undefined
  query.action = undefined
  dateRange.value = null
  query.page = 1
  fetchData()
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
