# NovaIIM IM服务器文档索引

> **注意：** 本目录为设计规划文档。
> 已实现服务：UserService、FriendService、MsgService、ConvService、GroupService、FileService、SyncService（265 测试用例全通过）。
> 已实现部分见 [todo.md](../todo.md)。

## 文档结构

| 文档 | 用途 | 状态 |
|------|------|------|
| [design.md](design.md) | 系统架构设计（Gateway / Service / DAO） | 已实现 (UserSvc/FriendSvc/MsgSvc/ConvSvc/GroupSvc/FileSvc/SyncSvc) |
| [api.md](api.md) | TCP 客户端集成指南 | 已实现 |
| [db_design.md](db_design.md) | IM 数据库表设计 | 已实现 |

**权威协议文档：** [protocol.md](../protocol.md)（命令字、消息体、错误码）

## 已实现

- Gateway（libhv TCP）、ConnManager、Router、ThreadPool
- UserService::Register / Login / Logout / Heartbeat / SearchUser / GetProfile / UpdateProfile
- FriendService::AddFriend / HandleRequest / DeleteFriend / Block / Unblock / GetFriendList / GetRequests
- MsgService::SendMsg / RecallMsg / DeliverAck / ReadAck
- ConvService::GetConvList / DeleteConv / MuteConv / PinConv / BroadcastConvUpdate
- GroupService::CreateGroup / DismissGroup / JoinGroup / HandleJoinReq / LeaveGroup / KickMember / UpdateGroup / GetGroupInfo / GetMembers / GetMyGroups / SetMemberRole / InviteToGroup
- FileService::Upload / UploadComplete / Download
- SyncService::SyncMessages / SyncUnread
- 心跳保活、设备管理

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

- **最后更新:** 2026-04-17
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

