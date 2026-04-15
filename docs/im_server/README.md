# NovaIIM IM服务器文档索引

欢迎查看 NovaIIM IM 服务器的完整文档。本目录包含关于架构设计、API 协议、数据库设计、测试和开发的全面指南。

## 📚 文档结构

### 1. 🏗️ [设计文档](design.md)

**用于:** 架构师、高级工程师、系统设计者

**内容:**
- 系统总体架构 (Gateway → Service → DAO → DB)
- 连接管理与进度流程
- 路由与分发机制
- 应用服务层 (UserService/MsgService/SyncService)
- 数据访问层 DAO 设计
- 线程模型与线程安全
- 错误处理机制
- 性能优化策略
- 扩展性与多机部署
- 监控告警指标

**关键部分:**
- 📊 系统架构图 (ASCII art)
- 📋 模块职责清单
- 🔄 运行流程图
- 🎯 Thread 模型详解

**快速浏览:** 30 分钟

---

### 2. 📡 [API 文档](api.md)

**用于:** 客户端开发者、接口使用者、测试工程师

**内容:**
- TCP 二进制帧格式详解
- 拆包机制 (libhv 配置)
- 11 个命令字完整说明:
  - 认证: Login/Logout/Heartbeat
  - 消息: SendMsg/DeliverAck/ReadAck
  - 同步: SyncMsg/SyncUnread
- 错误处理与错误码速查表
- 会话管理说明
- 客户端实现示例 (伪代码)
- 性能基准与指标

**关键部分:**
- 📦 帧格式示意图
- 📝 每个命令的 body 格式
- ✅ 成功/失败响应样例
- 📊 性能指标表

**快速浏览:** 45 分钟

**适合场景:**
- 实现客户端 SDK
- 调试协议问题
- 测试命令覆盖

---

### 3. 🗄️ [数据库设计](db_design.md)

**用于:** 数据库管理员、后端工程师、数据分析师

**内容:**
- 数据库总体设计原则
- 核心表定义与字段说明:
  - `users` 用户表
  - `conversations` 会话表
  - `messages` 消息表 (支持分区)
  - `user_devices` 设备表
  - `audit_logs` 审计表
- 外键关系与引用完整性
- 查询优化 (索引设计、慢查询检测)
- 性能基准与存储空间估算
- 备份与恢复策略
- 数据一致性保证
- 监控告警规则

**关键部分:**
- 📋 完整 Schema SQL (可直接运行)
- 🔍 索引设计建议
- 📊 查询性能表
- 💾 存储空间计算

**快速浏览:** 40 分钟

**适合场景:**
- 初始化数据库
- 优化慢查询
- 容量规划
- 备份恢复

---

### 4. ✅ [测试文档](testing.md)

**用于:** QA 工程师、测试开发者、CI/CD 工程师

**内容:**
- 测试策略与覆盖范围
- 单元测试框架与示例:
  - UserService 测试
  - MsgService 测试
  - SyncService 测试
  - DAO 层测试
- 集成测试场景脚本
- 协议测试方案
- 性能基准测试
- 故障恢复测试
- GitHub Actions CI/CD 配置
- 覆盖率计算方法

**关键部分:**
- 💻 完整的 C++ 测试代码
- 🐍 Python 集成测试脚本
- 📈 测试覆盖率目标
- ⚙️ CI/CD workflow 示例

**快速浏览:** 50 分钟

**适合场景:**
- 编写新测试
- 设置 CI/CD
- 性能基准验证
- 故障排查

---

### 5. ✨ [开发清单](development_checklist.md)

**用于:** 项目经理、技术主管、开发团队

**内容:**
- 项目概览与目标
- 6 个开发 Phase 进度跟踪
- 代码实现进度表
- 5 个 Sprint 规划与交付清单
- 功能完成度统计
- 代码质量标准
- 发布计划与版本规划
- 技术风险识别与缓解
- 成功标准检查清单
- Git 工作流与提交规范
- 代码审查清单
- 简要运维手册

**关键部分:**
- ✓ 功能完成度矩阵
- 📅 Sprint 时间表
- 🚨 风险与缓解表
- 📋 发布前检查清单

**快速浏览:** 35 分钟

**适合场景:**
- 项目规划与跟踪
- Sprint 规划会议
- 发布前检查
- 代码审查指导

---

## 🚀 快速开始

### for 客户端开发者

```bash
1. 阅读 API 文档 (15 分钟)
   → api.md 的 "帧格式" 和 "认证相关命令" 部分

2. 查看命令定义 (10 分钟)
   → api.md 的 "第 2-4 章" 了解所有命令

3. 实现客户端逻辑 (参考伪代码)
   → api.md 的 "第 7 章" 客户端实现示例

4. 测试协议 (可选)
   → testing.md 的 "协议测试" 部分
```

### for 后端工程师

```bash
1. 理解架构 (30 分钟)
   → design.md 完整阅读

2. 学习 DAO 设计 (15 分钟)
   → design.md 第 5 章 + db_design.md

3. 查看数据库 Schema (10 分钟)
   → db_design.md 附录 A 的完整 SQL

4. 实现新功能 (参考代码示例)
   → design.md 各 Service 的伪代码
   → development_checklist.md 的代码规范

5. 编写测试 (参考测试框架)
   → testing.md 的单元测试示例
```

### for QA/测试人员

```bash
1. 理解协议 (20 分钟)
   → api.md 第 1-2 章

2. 学习测试框架 (20 分钟)
   → testing.md 第 2-3 章

3. 设计测试用例 (30 分钟)
   → testing.md 第 6 章检查清单

4. 运行测试 (参考脚本)
   → testing.md 的运行指令
```

### for 项目经理/Tech Lead

```bash
1. 项目概览 (15 分钟)
   → development_checklist.md 第 1-2 章

2. 跟踪进度 (10 分钟)
   → development_checklist.md 第 3-4 章

3. Sprint 规划 (20 分钟)
   → development_checklist.md 第 4 章

4. 发布检查 (15 分钟)
   → development_checklist.md "0.3.0 发布清单"
```

---

## 📊 文档统计

| 文档 | 字数 | 代码行 | 主要内容 |
|------|------|--------|---------|
| design.md | ~18,000 | 100+ | 架构设计、模块职责 |
| api.md | ~15,000 | 200+ | 协议详解、命令说明 |
| db_design.md | ~12,000 | 150+ | 表设计、查询优化 |
| testing.md | ~16,000 | 300+ | 测试框架、示例代码 |
| development_checklist.md | ~14,000 | 50+ | 进度跟踪、规范方案 |
| **总计** | **~75,000** | **800+** | **完整IM服务器文档** |

---

## 🔍 按主题快速查找

### 架构与设计
- 系统架构图 → [design.md](design.md) 第 1.1 章
- 连接生命周期 → [design.md](design.md) 第 2 章
- 线程模型 → [design.md](design.md) 第 6 章

### 协议与通信
- 帧格式定义 → [api.md](api.md) 第 1.1 章
- 所有命令字 → [api.md](api.md) 目录
- 错误处理 → [api.md](api.md) 第 5 章

### 数据库
- 表结构定义 → [db_design.md](db_design.md) 第 2 章
- 完整 Schema → [db_design.md](db_design.md) 附录 A
- 查询优化 → [db_design.md](db_design.md) 第 4 章

### 测试
- 单元测试示例 → [testing.md](testing.md) 第 2 章
- 集成测试脚本 → [testing.md](testing.md) 第 3 章
- 性能测试 → [testing.md](testing.md) 第 5 章

### 开发流程
- 代码规范 → [development_checklist.md](development_checklist.md) 第 5 章
- Git 工作流 → [development_checklist.md](development_checklist.md) 第 9 章
- Sprint 计划 → [development_checklist.md](development_checklist.md) 第 4 章

---

## ❓ 常见问题

**Q: 我是新成员，应该从何开始？**  
A: 按以下顺序阅读：
1. [development_checklist.md](development_checklist.md) - 项目概览
2. [design.md](design.md) - 架构理解
3. 根据角色选择专项文档

**Q: 如何实现一个新的 IM 命令？**  
A: 参考 [development_checklist.md](development_checklist.md) 第 12.2 章 "快速开发流程"

**Q: 为什么我的查询很慢？**  
A: 查看 [db_design.md](db_design.md) 第 4 章 "查询优化"

**Q: 如何调试协议问题？**  
A: 参考 [api.md](api.md) 第 9 章 "故障排查"

**Q: 新增功能应该写哪些测试？**  
A: 参考 [testing.md](testing.md) 第 6 章 "测试清单"

---

## 🔗 相关文档

其他重要文档：
- Admin Server 文档 → `docs/admin_server/`
- 测试总结 → `docs/TESTING_SUMMARY.md`
- 测试指南 → `docs/ADMIN_TEST_GUIDE.md`
- 原始协议文档 → `docs/protocol.md`
- 数据库设计 → `docs/db_design.sql`

---

## ✏️ 维护与反馈

- **最后更新:** 2026-04-15
- **维护者:** NovaIIM 开发团队
- **反馈:** 发现错误或有改进建议？提交 Issue 或 PR

---

## 📖 推荐阅读顺序

```
新手入门:
[项目概览] → [架构设计] → [协议理解] → [实际编码]
    ↓           ↓           ↓            ↓
[Development] → [Design]  → [API]   → [选择专项文档]

专项深入:
- 后端工程师: Design → DB_Design → Testing → Development
- 客户端开发: API → Testing (Protocol 部分) → Development
- 测试工程师: API → Testing → Development
- DBA/运维: DB_Design → Development
```

---

**祝好！开启 NovaIIM IM 服务器开发之旅！🚀**

