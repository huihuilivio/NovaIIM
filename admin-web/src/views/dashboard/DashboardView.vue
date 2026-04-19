<template>
  <div class="dashboard">
    <el-row :gutter="20" class="stat-cards">
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background: #409eff"><el-icon :size="28"><Connection /></el-icon></div>
          <div class="stat-body">
            <div class="stat-value">{{ stats.connections }}</div>
            <div class="stat-label">当前连接数</div>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background: #67c23a"><el-icon :size="28"><User /></el-icon></div>
          <div class="stat-body">
            <div class="stat-value">{{ stats.online_users }}</div>
            <div class="stat-label">在线用户</div>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background: #e6a23c"><el-icon :size="28"><ChatDotRound /></el-icon></div>
          <div class="stat-body">
            <div class="stat-value">{{ stats.messages_today }}</div>
            <div class="stat-label">今日消息</div>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card">
          <div class="stat-icon" style="background: #909399"><el-icon :size="28"><Timer /></el-icon></div>
          <div class="stat-body">
            <div class="stat-value">{{ uptimeText }}</div>
            <div class="stat-label">运行时长</div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="20" style="margin-top: 20px">
      <el-col :span="12">
        <el-card shadow="hover">
          <template #header>系统资源</template>
          <el-descriptions :column="1" border>
            <el-descriptions-item label="CPU 使用率">{{ stats.cpu_percent }}%</el-descriptions-item>
            <el-descriptions-item label="内存使用">{{ stats.memory_mb }} MB</el-descriptions-item>
            <el-descriptions-item label="异常包数">{{ stats.bad_packets }}</el-descriptions-item>
          </el-descriptions>
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="hover">
          <template #header>快捷操作</template>
          <div class="quick-actions">
            <el-button type="primary" @click="refresh" :loading="loading">刷新数据</el-button>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { getDashboardStats } from '@/api/dashboard'
import type { DashboardStats } from '@/api/dashboard'
import { ElMessage } from 'element-plus'

const loading = ref(false)
const stats = ref<DashboardStats>({
  connections: 0,
  online_users: 0,
  messages_today: 0,
  bad_packets: 0,
  uptime_seconds: 0,
  cpu_percent: 0,
  memory_mb: 0,
  timestamp: '',
})

const uptimeText = computed(() => {
  const s = stats.value.uptime_seconds
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  if (d > 0) return `${d}天${h}时${m}分`
  if (h > 0) return `${h}时${m}分`
  return `${m}分`
})

async function refresh() {
  loading.value = true
  try {
    const res = await getDashboardStats()
    if (res.data.code === 0) {
      stats.value = res.data.data
    }
  } catch {
    ElMessage.error('获取仪表盘数据失败')
  } finally {
    loading.value = false
  }
}

onMounted(refresh)
</script>

<style scoped lang="scss">
.stat-cards {
  .stat-card {
    :deep(.el-card__body) {
      display: flex;
      align-items: center;
      gap: 16px;
      padding: 20px;
    }
  }
}

.stat-icon {
  width: 56px;
  height: 56px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  flex-shrink: 0;
}

.stat-body {
  .stat-value {
    font-size: 24px;
    font-weight: 600;
    color: #303133;
  }
  .stat-label {
    font-size: 13px;
    color: #909399;
    margin-top: 4px;
  }
}

.quick-actions {
  display: flex;
  gap: 12px;
}
</style>
