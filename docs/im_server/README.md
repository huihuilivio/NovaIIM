# NovaIIM IM服务器文档索引

> IM 服务器设计文档，实现进度见 [todo.md](../todo.md)。

## 文档结构

| 文档 | 用途 |
|------|------|
| [design.md](design.md) | 系统架构设计（Gateway / Service / DAO） |
| [api.md](api.md) | TCP 客户端集成指南 |
| [db_design.md](db_design.md) | IM 数据库表设计 |

**权威协议文档：** [protocol.md](../protocol.md)（命令字、消息体、错误码）

## 已实现

- Gateway（libhv TCP）、ConnManager、Router、ThreadPool
- UserService::Register / Login / Logout / Heartbeat / SearchUser / GetProfile / UpdateProfile
- FriendService::AddFriend / HandleRequest / DeleteFriend / Block / Unblock / GetFriendList / GetRequests
- MsgService::SendMsg / RecallMsg / DeliverAck / ReadAck
- ConvService::GetConvList / DeleteConv / MuteConv / PinConv / BroadcastConvUpdate
- GroupService::CreateGroup / DismissGroup / JoinGroup / HandleJoinReq / LeaveGroup / KickMember / UpdateGroup / GetGroupInfo / GetMembers / GetMyGroups / SetMemberRole / InviteToGroup
- FileService::Upload / UploadComplete / Download
- FileServer（HTTP 文件上传/下载/删除，端口 9092）
- SyncService::SyncMessages / SyncUnread
- 心跳保活、设备管理

---

## 常见问题

**Q: 我是新成员，应该从何开始？**  
A: 按以下顺序阅读：
1. [design.md](design.md) - 架构理解
2. [api.md](api.md) - TCP 协议集成
3. [db_design.md](db_design.md) - 数据库设计

**Q: 如何实现一个新的 IM 命令？**  
A: 参考 [design.md](design.md) Service 层设计和 [../protocol.md](../protocol.md) 协议定义

**Q: 为什么我的查询很慢？**  
A: 查看 [db_design.md](db_design.md) 索引设计和查询优化

**Q: 如何调试协议问题？**  
A: 参考 [api.md](api.md) 连接生命周期和 [../protocol.md](../protocol.md)

---

## 相关文档

- Admin Server 文档 → `docs/admin_server/`
- 协议文档 → `docs/protocol.md`
- 数据库设计 → `docs/db_design.sql`
- 服务端架构 → `docs/server_arch.md`
- 系统架构 → `docs/architecture.md`

---

## 推荐阅读顺序

- **后端工程师**: design.md → db_design.md → protocol.md
- **客户端开发**: api.md → protocol.md → sdk/README.md
- **DBA/运维**: db_design.md → server_arch.md

