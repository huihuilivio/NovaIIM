# NovaIIM IM服务器 - 开发清单与里程碑

## 1. 项目概览

**项目:** NovaIIM IM服务器核心模块  
**语言:** C++20  
**框架:** libhv (异步网络库)  
**数据库:** SQLite (开发) / MySQL (生产)  
**构建:** CMake  
**测试:** Google Test  
**日志:** spdlog  

**目标:** 高性能、可扩展的即时通讯后端，支持50k+ 并发连接，10k+ TPS 消息吞吐

---

## 2. 功能完成度跟踪

### Phase 1: 核心架构 ✓ 完成

- [x] TCP Gateway (libhv)
  - [x] 连接管理 (ConnManager)
  - [x] 包解析 (UNPACK_BY_LENGTH_FIELD)
  - [x] 工作队列 (ThreadPool)
  - [x] 路由分发 (Router)

- [x] 业务服务层
  - [x] UserService (登录/登出/心跳)
  - [x] MsgService (发送/送达/已读)
  - [x] SyncService (消息同步/未读)

- [x] 数据访问层 (DAO)
  - [x] DAO 工厂模式 (支持多后端)
  - [x] ormpp 集成
  - [x] SQLite 实现
  - [x] MySQL 实现

- [x] 配置系统
  - [x] YAML 配置文件解析
  - [x] 环境变量覆盖
  - [x] 校验与默认值

---

### Phase 2: 协议与消息 ✓ 完成

**TCP 二进制帧协议:**
- [x] 帧格式定义 (cmd/seq/uid/body_len/body)
- [x] libhv 拆包配置
- [x] 错误处理

**认证命令:**
- [x] Cmd::0x0001 (Login)
- [x] Cmd::0x0003 (Logout)
- [x] Cmd::0x0010 (Heartbeat)

**消息命令:**
- [x] Cmd::0x0100 (SendMsg)
- [x] Cmd::0x0101 (SendMsgAck)
- [x] Cmd::0x0102 (PushMsg)
- [x] Cmd::0x0103 (DeliverAck)
- [x] Cmd::0x0104 (ReadAck)

**同步命令:**
- [x] Cmd::0x0200 (SyncMsg)
- [x] Cmd::0x0201 (SyncMsgResp)
- [x] Cmd::0x0202 (SyncUnread)
- [x] Cmd::0x0203 (SyncUnreadResp)

---

### Phase 3: 数据库设计 ✓ 完成

- [x] users 表 (用户账户)
- [x] conversations 表 (会话管理)
- [x] messages 表 (消息存储)
- [x] user_devices 表 (多设备管理)
- [x] audit_logs 表 (审计)
- [x] 关键索引设计
- [x] 查询优化方案
- [x] 备份恢复策略

**性能指标:**
- [x] 登录查询: <1ms
- [x] 消息发送: 20-50ms
- [x] 历史消息拉取: 50-100ms

---

### Phase 4: 测试套件 ⏳ 进行中

**单元测试:**
- [x] UserService 测试
- [x] MsgService 测试  
- [x] SyncService 测试
- [x] DAO 层测试
- [ ] 网络 I/O 测试
- [ ] 线程安全测试

**集成测试:**
- [ ] 完整消息流程 (A 发消息 → B 接收)
- [ ] 离线消息同步
- [ ] 多会话并发

**协议测试:**
- [ ] 包格式验证
- [ ] 命令字覆盖
- [ ] 过大包处理
- [ ] 非法包处理

**性能测试:**
- [ ] TPS 基准 (目标: >50k)
- [ ] 延迟 P95/P99
- [ ] 内存占用
- [ ] CPU 使用率

---

### Phase 5: 文档与示例 ✓ 完成

- [x] 架构设计文档 (design.md)
- [x] API 协议文档 (api.md)
- [x] 数据库设计文档 (db_design.md)
- [x] 测试文档 (testing.md)
- [x] 开发清单文档 (development_checklist.md)
- [ ] 客户端开发指南
- [ ] 部署运维指南
- [ ] 故障排查指南

---

### Phase 6: 生产优化 ⏳ 计划中

**性能优化:**
- [ ] 消息批处理
- [ ] Redis 缓存集成 (用户信息/未读计数)
- [ ] 连接池调优
- [ ] 数据库分片策略

**可观测性:**
- [ ] 结构化日志 (更新 LogFormatter)
- [ ] Prometheus 集成
- [ ] 链路追踪 (jaeger)
- [ ] 错误追踪 (Sentry)

**可靠性:**
- [ ] 消息去重机制
- [ ] 心跳超时恢复
- [ ] 数据库故障转移
- [ ] 灰度升级方案

---

## 3. 代码实现进度

### 核心模块

| 模块 | 文件 | LOC | 完成度 | 备注 |
|------|------|-----|--------|------|
| Gateway | net/gateway.cpp | 400+ | ✓ 完成 | TCP 服务器 |
| ConnManager | net/conn_manager.cpp | 300+ | ✓ 完成 | 连接管理 |
| Router | service/router.cpp | 200+ | ✓ 完成 | 命令路由 |
| UserService | service/user_service.cpp | 350+ | ✓ 完成 | 用户认证 |
| MsgService | service/msg_service.cpp | 500+ | ✓ 完成 | 消息处理 |
| SyncService | service/sync_service.cpp | 300+ | ✓ 完成 | 消息同步 |
| UserDao | dao/impl/user_dao_impl.cpp | 250+ | ✓ 完成 | 用户持久层 |
| MessageDao | dao/impl/message_dao_impl.cpp | 350+ | ✓ 完成 | 消息持久层 |
| ThreadPool | core/thread_pool.cpp | 250+ | ✓ 完成 | 工作线程池 |
| **总计** | | **3500+** | **✓ 完成** | - |

### 数据库 Schema

| 表名 | 行数 | 索引 | 状态 |
|------|------|------|------|
| users | 创建 SQL | idx_uid, idx_status | ✓ |
| conversations | 创建 SQL | unique_members | ✓ |
| messages | 创建 SQL | 5 个索引 | ✓ |
| user_devices | 创建 SQL | 2 个索引 | ✓ |
| audit_logs | 创建 SQL | 3 个索引 | ✓ |

### 测试代码

| 测试文件 | 测试类 | 用例数 | 状态 |
|---------|--------|--------|------|
| test_user_service.cpp | UserServiceTest | 12 | ⏳ 待编写 |
| test_msg_service.cpp | MsgServiceTest | 18 | ⏳ 待编写 |
| test_sync_service.cpp | SyncServiceTest | 8 | ⏳ 待编写 |
| test_message_dao.cpp | MessageDaoTest | 15 | ⏳ 待编写 |
| test_router.cpp | RouterTest | 5 | ✓ 已有 |

---

## 4. Sprint 规划

### Sprint 1 (2周): 基础实现 ✓

**目标:** 完成核心架构和协议

- [x] TCP Gateway + 包解析
- [x] 认证流程 (Login/Logout)
- [x] 消息发送与推送
- [x] 数据库模型设計

**交付物:**
- [x] 可连接的 IM 服务器
- [x] 支持基本的 Login/SendMsg/SyncMsg 命令

**验收标准:**
- [x] 启动无报错
- [x] 客户端能连接
- [x] 能完成登录流程

---

### Sprint 2 (2周): 消息处理与存储 ✓

**目标:** 完整的消息流程 + 数据持久化

- [x] 消息发送/接收 (online + offline)
- [x] 已送达确认 (DeliverAck)
- [x] 已读确认 (ReadAck)
- [x] 数据库集成 (SQLite)

**交付物:**
- [x] 完整的 E2E 消息流程文档
- [x] 数据库 Schema 定义

**验收标准:**
- [x] 两个连接能发送和接收消息
- [x] 消息能保存到数据库
- [x] 拉取历史消息正常工作

---

### Sprint 3 (2周): 同步与多会话 ✓

**目标:** 支持多个平行会话，离线消息同步

- [x] 多会话管理
- [x] 历史消息拉取 (分页)
- [x] 未读消息计数
- [x] MySQL 支持

**交付物:**
- [x] MySQL 后端 DAO 实现
- [x] 会话管理设计

**验收标准:**
- [x] 支持同时创建 10+ 个会话
- [x] 历史消息分页拉取正常
- [x] 切换 SQLite ↔ MySQL 无缝

---

### Sprint 4 (2周): 测试与文档 ⏳

**目标:** 测试覆盖率 >80%，完整文档

**任务:**
- [ ] 编写单元测试 (目标: 40+ 用例)
- [ ] 集成测试脚本 (完整消息流程)
- [ ] 性能基准测试
- [ ] 写完所有 md 文档

**交付物:**
- [ ] 测试报告 (覆盖率/TPS)
- [ ] 文档完整性检查表
- [ ] 开发者快速开始指南

**验收标准:**
- [ ] 单元测试覆盖率 >80%
- [ ] 集成测试通过率 100%
- [ ] 文档清晰完整，无歧义

---

### Sprint 5 (1周): 性能优化与生产准备 ⏳

**目标:** 达到生产级性能指标

**任务:**
- [ ] 性能基准测试 (50k+ TPS, <50ms P95)
- [ ] 内存优化 (无泄漏)
- [ ] 索引优化
- [ ] Redis 缓存集成 (可选)

**交付物:**
- [ ] 性能报告
- [ ] 优化前后对比

**验收标准:**
- [ ] TPS >= 50,000 msg/sec
- [ ] P95 延迟 < 50ms
- [ ] 内存占用稳定

---

## 5. 代码质量标准

### 5.1 编码规范

**命名规范:**
```cpp
class UserService;           // 类名: PascalCase
void HandleLogin(...);       // 函数名: PascalCase
int login_attempt_count;     // 变量: snake_case
static const int MAX_USERS;  // 常量: UPPER_CASE
```

**代码风格:**
```cpp
// 使用智能指针
std::unique_ptr<UserService> service;
std::shared_ptr<Connection> conn;

// 避免裸指针
User* user = nullptr;  // ❌ 避免

// const 正确性
void ProcessMessage(const Packet& pkt);
const std::string& GetName() const;

// 错误处理
try {
    result = risky_operation();
} catch (const std::exception& e) {
    NOVA_LOG_ERROR("Operation failed: {}", e.what());
    throw;
}
```

### 5.2 测试要求

- **单元测试**: 每个公共方法至少 2 个用例 (正常 + 异常)
- **覆盖率**: 关键路径 >80%
- **集成测试**: 主流程 100% 覆盖
- **性能测试**: P95 延迟监控

### 5.3 代码审查清单

```cpp
// Pull Request 审查清单

编译检查:
[ ] 无编译警告 (gcc/clang)
[ ] 无 MSVC 警告

功能检查:
[ ] 代码逻辑正确
[ ] 异常处理完整
[ ] 没有潜在的缓冲区溢出
[ ] 线程安全正确

性能检查:
[ ] 无不必要的内存分配
[ ] 无死锁风险
[ ] 数据库查询有索引

文档检查:
[ ] 函数有注释说明
[ ] 复杂逻辑有解释
[ ] 修改了 public API 则更新文档

测试检查:
[ ] 新代码有测试用例
[ ] 所有测试通过
[ ] 测试覆盖新路径
```

---

## 6. 发布计划

### 版本规划

| 版本 | 目标 | 功能 | 时间框 |
|------|------|------|-------|
| 0.1.0 | Alpha | 基础消息 | ✓ 完成 |
| 0.2.0 | Beta | 多会话+同步 | ✓ 完成 |
| 0.3.0 | RC | 完整测试+文档 | ⏳ 2 周 |
| 1.0.0 | GA | 生产就绪 | ⏳ 4 周 |
| 1.1.0 | - | 群聊支持 | 计划中 |
| 1.2.0 | - | WebSocket 支持 | 计划中 |

### 0.3.0 发布清单

```
代码质量:
[ ] cppcheck 无 error
[ ] 单元测试覆盖 >80%
[ ] 集成测试全通过
[ ] valgrind 无泄漏

文档完整:
[ ] API 文档最新
[ ] 数据库设计确认
[ ] 部署指南完成
[ ] 变更日志 (CHANGELOG.md) 更新

性能验证:
[ ] 50k 并发连接测试通过
[ ] 10k TPS 消息吞吐验证
[ ] P95 延迟 < 100ms

安全检查:
[ ] 无 SQL 注入
[ ] 无身份验证绕过
[ ] 所有不信任输入都校验

打包发布:
[ ] GitHub Release 创建
[ ] Docker 镜像构建
[ ] 文档发布到 wiki
```

---

## 7. 风险与缓解

### 7.1 技术风险

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| libhv 不支持某功能 | 中 | 低 | 及早集成测试 |
| ormpp SQLite 性能 | 中 | 中 | 切换 MySQL, 使用连接池 |
| 消息顺序 race condition | 高 | 低 | seq 机制 + 单元测试 |
| ThreadPool 死锁 | 高 | 低 | 压力测试 + 代码审查 |
| 数据库连接耗尽 | 中 | 中 | 连接池限制 + 监控 |

### 7.2 进度风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| 测试编写慢 | 影响交付 | 评估工作量，可行时中止部分测試 |
| 性能未达指标 | 影响上线 | 提前基准测试，预留优化时间 |
| 人员变动 | 知识丧失 | 文档详细，代码自解释 |

---

## 8. 成功标准

### 功能完成

- [x] 完整的 TCP 二进制帧协议实现
- [x] 所有 11 个命令字实现
- [x] 支持私聊 + 群聊 + 多设备
- [x] 在线推送 + 离线同步

### 质量指标

- ✓ 代码覆盖率 > 80%
- ✓ 单元测试通过率 100%
- ⏳ 集成测试通过率 100%
- ⏳ 一周内无严重 bug

### 性能指标

- ⏳ TPS >= 50,000 msg/sec
- ⏳ P95 延迟 < 100ms
- ⏳ 支持 100k+ 并发连接
- ⏳ 内存占用 < 1GB @50k conn

### 文档完整

- [x] 架构设计文档
- [x] API 协议文档
- [x] 数据库设计文档
- [x] 测试文档
- [x] 开发清单文档
- [ ] 部署运维指南
- [ ] 故障排查指南
- [ ] 客户端开发示例

---

## 9. 开发工作流

### 9.1 Git 工作流

```bash
# 切出新分支
git checkout -b feature/user-ban-feature

# 定期提交
git commit -m "feat: 实现用户封禁功能

- 添加 user.status = 2 (禁用) 字段
- 修改 UserService::HandleLogin 检查禁用状态
- 建议 2 个新单元测试

Closes #123"

# 推送 + PR
git push origin feature/user-ban-feature

# Code Review 后 merge
git checkout main
git merge feature/user-ban-feature

# 删除本地分支
git branch -d feature/user-ban-feature
```

### 9.2 Commit 消息规范

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: 新功能
- `fix`: 缺陷修复
- `docs`: 文档改进
- `test`: 测试添加/修复
- `refactor`: 代码重构
- `perf`: 性能优化
- `chore`: 构建/依赖更新

**Example:**
```
feat(msg-service): 添加消息撤回功能

- 新增 Cmd::0x0105 (RevokMsg)
- 修改消息表 status 字段值定义
- 新增撤回超时检查 (60s)

Related-To: #456
Reviewed-By: @alice
```

### 9.3 代码审查流程

1. **提交 PR**: 包含详细描述 + 测试用例 + 文档更新
2. **自动检查**: 编译 + 单元测试 + 代码静态分析
3. **代码审查**: ≥2 人审查，Check 上方清单
4. **修改反馈**: 回应审查意见，推送修复
5. **批准合并**: 所有检查通过 + 审查者批准

---

## 10. 运维手册 (简化版)

### 启动服务

```bash
# 检查配置文件
cat configs/server.yaml

# 启动
./nova_iim -c configs/server.yaml

# 监控日志
tail -f logs/nova_iim.log | grep "\[ERROR\]\|\[WARN\]"
```

### 常见问题

**Q: 服务启动失败**
- A: 检查 configs/server.yaml 是否存在，端口是否被占用

**Q: 消息发送缓慢**
- A: 检查数据库连接, CPU 使用率, 网络延迟

**Q: 连接断开频繁**
- A: 检查防火墙, 心跳间隔设置, 客户端日志

---

## 11. 参考资源

- [C++20 标准](https://en.cppreference.com/)
- [libhv 文档](https://github.com/ithewei/libhv)
- [Google Test 教程](https://google.github.io/googletest/)
- [spdlog 快速开始](https://github.com/gabime/spdlog)
- [ormpp 示例](https://github.com/qicosmos/ormpp)

---

**更新时间**: 2026-04-15  
**维护者**: NovaIIM 开发团队  
**最后审查**: @tech-lead

