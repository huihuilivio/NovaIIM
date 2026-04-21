<template>
  <div class="message-list">
    <el-card shadow="never" class="filter-card">
      <el-form :inline="true" @submit.prevent="handleSearch">
        <el-form-item label="会话ID">
          <el-input v-model.number="query.conversation_id" placeholder="输入会话 ID" clearable />
        </el-form-item>
        <el-form-item label="时间范围">
          <el-date-picker
            v-model="dateRange"
            type="datetimerange"
            range-separator="至"
            start-placeholder="开始时间"
            end-placeholder="结束时间"
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
        <el-table-column prop="conversation_id" label="会话" width="100" />
        <el-table-column prop="sender_uid" label="发送者" width="160" />
        <el-table-column prop="content" label="内容" show-overflow-tooltip />
        <el-table-column label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.status === 3 ? 'info' : 'success'" size="small">
              {{ row.status === 3 ? '已撤回' : '正常' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="seq" label="Seq" width="80" />
        <el-table-column prop="created_at" label="时间" width="180" />
        <el-table-column label="操作" width="100" fixed="right">
          <template #default="{ row }">
            <el-button size="small" type="warning" :disabled="row.status === 3" @click="handleRecall(row)">
              撤回
            </el-button>
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
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { getMessages, recallMessage } from '@/api/message'
import type { Message } from '@/api/message'
import { ElMessage, ElMessageBox } from 'element-plus'

const loading = ref(false)
const tableData = ref<Message[]>([])
const total = ref(0)
const dateRange = ref<[string, string] | null>(null)
const query = reactive({
  page: 1,
  page_size: 20,
  conversation_id: undefined as number | undefined,
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
    const res = await getMessages(query)
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
  query.conversation_id = undefined
  dateRange.value = null
  query.page = 1
  fetchData()
}

async function handleRecall(row: Message) {
  const { value } = await ElMessageBox.prompt('请输入撤回原因', '撤回消息')
  const res = await recallMessage(row.id, value)
  if (res.data.code === 0) {
    ElMessage.success('已撤回')
    fetchData()
  } else {
    ElMessage.error(res.data.msg)
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
