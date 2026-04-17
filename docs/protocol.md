# NovaIIM 协议设计

## 1. TCP 二进制帧协议

客户端与 Gateway 之间的通信协议，基于 TCP 长连接。

### 帧格式（小端序）

```
+-------+-------+-------+------------+------+
| cmd:2 | seq:4 | uid:8 | body_len:4 | body |
+-------+-------+-------+------------+------+
  总计: 18 字节固定头 + body_len 字节 body
```

| 字段 | 类型 | 字节 | 说明 |
|------|------|------|------|
| cmd | uint16 | 2 | 命令字，见下方定义 |
| seq | uint32 | 4 | 客户端序列号（请求/响应配对） |
| uid | uint64 | 8 | 用户 ID（登录后由服务端认证） |
| body_len | uint32 | 4 | body 长度（最大 1 MB） |
| body | bytes | N | 业务数据（格式取决于 cmd） |

### 拆包机制

libhv `UNPACK_BY_LENGTH_FIELD`:
- `length_field_offset = 14` (cmd + seq + uid)
- `length_field_bytes = 4`
- `length_field_coding = ENCODE_BY_LITTEL_ENDIAN`
- `body_offset = 18` (固定头大小)
- `package_max_length = 18 + 1MB`

### 安全说明

- 服务端不信任帧头中的 `uid` 字段，心跳等操作使用 `conn->user_id()`（登录后服务端设置）
- 未认证连接的 `user_id()` 为 0

---

## 2. 命令字定义

| 命令 | 值 | 方向 | 说明 |
|------|------|------|------|
| **认证** |
| kLogin | 0x0001 | C→S | 登录请求 |
| kLoginAck | 0x0002 | S→C | 登录响应 |
| kLogout | 0x0003 | C→S | 登出请求 |
| kRegister | 0x0004 | C→S | 注册请求 |
| kRegisterAck | 0x0005 | S→C | 注册响应 |
| **心跳** |
| kHeartbeat | 0x0010 | C→S | 心跳请求 |
| kHeartbeatAck | 0x0011 | S→C | 心跳响应 |
| **个人资料** |
| kGetUserProfile | 0x0302 | C→S | 获取用户资料 |
| kGetUserProfileAck | 0x0303 | S→C | 用户资料响应 |
| **用户搜索 / 资料编辑** |
| kSearchUser | 0x0400 | C→S | 搜索用户（by email / nickname） |
| kSearchUserAck | 0x0401 | S→C | 搜索用户响应 |
| kUpdateProfile | 0x0402 | C→S | 修改个人资料（昵称/头像） |
| kUpdateProfileAck | 0x0403 | S→C | 修改资料响应 |
| **好友** |
| kAddFriend | 0x0030 | C→S | 发起好友申请 |
| kAddFriendAck | 0x0031 | S→C | 好友申请响应 |
| kHandleFriendReq | 0x0032 | C→S | 处理好友申请（同意/拒绝） |
| kHandleFriendReqAck | 0x0033 | S→C | 处理好友申请响应 |
| kDeleteFriend | 0x0034 | C→S | 删除好友 |
| kDeleteFriendAck | 0x0035 | S→C | 删除好友响应 |
| kBlockFriend | 0x0036 | C→S | 拉黑用户 |
| kBlockFriendAck | 0x0037 | S→C | 拉黑响应 |
| kUnblockFriend | 0x0038 | C→S | 取消拉黑 |
| kUnblockFriendAck | 0x0039 | S→C | 取消拉黑响应 |
| kGetFriendList | 0x003A | C→S | 获取好友列表 |
| kGetFriendListAck | 0x003B | S→C | 好友列表响应 |
| kGetFriendRequests | 0x003C | C→S | 获取好友申请列表 |
| kGetFriendRequestsAck | 0x003D | S→C | 好友申请列表响应 |
| kFriendNotify | 0x003E | S→C | 好友变更推送（申请/同意/删除/拉黑） |
| **消息** |
| kSendMsg | 0x0100 | C→S | 发送消息 |
| kSendMsgAck | 0x0101 | S→C | 发送确认 (含 seq) |
| kPushMsg | 0x0102 | S→C | 推送消息 |
| kDeliverAck | 0x0103 | C→S | 已送达确认 |
| kReadAck | 0x0104 | C→S | 已读确认 |
| kRecallMsg | 0x0105 | C→S | 撤回消息 |
| kRecallMsgAck | 0x0106 | S→C | 撤回响应 |
| kRecallNotify | 0x0107 | S→C | 撤回推送（通知会话其他成员） |
| **会话** |
| kCreateConv | 0x0110 | C→S | 创建会话（开始私聊自动创建） |
| kCreateConvAck | 0x0111 | S→C | 创建会话响应 |
| kGetConvList | 0x0112 | C→S | 获取会话列表 |
| kGetConvListAck | 0x0113 | S→C | 会话列表响应 |
| kDeleteConv | 0x0114 | C→S | 删除（隐藏）会话 |
| kDeleteConvAck | 0x0115 | S→C | 删除会话响应 |
| kMuteConv | 0x0116 | C→S | 设置会话免打扰 |
| kMuteConvAck | 0x0117 | S→C | 免打扰响应 |
| kPinConv | 0x0118 | C→S | 置顶会话 |
| kPinConvAck | 0x0119 | S→C | 置顶响应 |
| kConvUpdate | 0x011A | S→C | 会话变更推送（新消息摘要/成员变化等） |
| **同步** |
| kSyncMsg | 0x0200 | C→S | 拉取历史消息 |
| kSyncMsgResp | 0x0201 | S→C | 历史消息响应 |
| kSyncUnread | 0x0202 | C→S | 拉取未读 |
| kSyncUnreadResp | 0x0203 | S→C | 未读响应 |
| **群组** |
| kCreateGroup | 0x0400 | C→S | 建群 |
| kCreateGroupAck | 0x0401 | S→C | 建群响应 |
| kDismissGroup | 0x0402 | C→S | 解散群（仅群主） |
| kDismissGroupAck | 0x0403 | S→C | 解散群响应 |
| kJoinGroup | 0x0404 | C→S | 申请入群 |
| kJoinGroupAck | 0x0405 | S→C | 入群响应 |
| kHandleJoinReq | 0x0406 | C→S | 处理入群申请（群主/管理员） |
| kHandleJoinReqAck | 0x0407 | S→C | 处理入群申请响应 |
| kLeaveGroup | 0x0408 | C→S | 退出群 |
| kLeaveGroupAck | 0x0409 | S→C | 退群响应 |
| kKickMember | 0x040A | C→S | 踢出成员 |
| kKickMemberAck | 0x040B | S→C | 踢出响应 |
| kGetGroupInfo | 0x040C | C→S | 获取群信息 |
| kGetGroupInfoAck | 0x040D | S→C | 群信息响应 |
| kUpdateGroup | 0x040E | C→S | 修改群信息（名称/头像/公告） |
| kUpdateGroupAck | 0x040F | S→C | 修改群信息响应 |
| kGetGroupMembers | 0x0410 | C→S | 获取群成员列表 |
| kGetGroupMembersAck | 0x0411 | S→C | 群成员列表响应 |
| kGetMyGroups | 0x0412 | C→S | 获取我加入的群列表 |
| kGetMyGroupsAck | 0x0413 | S→C | 我的群列表响应 |
| kSetMemberRole | 0x0414 | C→S | 设置群成员角色（管理员/普通） |
| kSetMemberRoleAck | 0x0415 | S→C | 设置角色响应 |
| kGroupNotify | 0x0416 | S→C | 群事件推送（成员变化/群信息变更/解散） |
| **文件** |
| kUploadReq | 0x0500 | C→S | 文件上传请求（元数据） |
| kUploadAck | 0x0501 | S→C | 上传凭证响应（upload_url / file_id） |
| kUploadComplete | 0x0502 | C→S | 上传完成通知 |
| kUploadCompleteAck | 0x0503 | S→C | 上传完成确认 |
| kDownloadReq | 0x0504 | C→S | 文件下载请求 |
| kDownloadAck | 0x0505 | S→C | 下载 URL 响应 |

---

## 3. 消息体定义

### 3.1 注册 (kRegister / kRegisterAck)

**RegisterReq** (C→S, 0x0004):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| email | string | ✅ | 登录邮箱，最长 255 字符，不区分大小写，格式校验 |
| nickname | string | ✅ | 昵称，可重复，最长 100 字符，自动 trim 首尾空白，禁止控制字符 |
| password | string | ✅ | 密码，6–128 字符 |

**RegisterAck** (S→C, 0x0005):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功，>0 错误 |
| msg | string | 错误描述 |
| uid | string | 服务端生成的唯一 UID（Snowflake） |

**注册错误码:**

| code | 含义 |
|------|------|
| 1001 | 邮箱为空 |
| 1006 | 邮箱格式无效 |
| 1007 | 邮箱超过 255 字符 |
| 1008 | 邮箱已注册 |
| 1010 | 昵称为空 |
| 1011 | 昵称超过 100 字符 |
| 1012 | 昵称包含非法字符（控制字符） |
| 1013 | 密码少于 6 字符 |
| 1014 | 密码超过 128 字符 |
| 1016 | 注册失败（服务端内部错误） |

### 3.2 登录 (kLogin / kLoginAck)

**LoginReq** (C→S, 0x0001):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| email | string | ✅ | 登录邮箱（不区分大小写） |
| password | string | ✅ | 密码 |
| device_id | string | | 设备标识（用于多端管理） |
| device_type | string | | 设备类型："pc", "mobile", "web" 等 |

**LoginAck** (S→C, 0x0002):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功，>0 错误 |
| msg | string | 错误描述 |
| uid | string | 用户 UID（Snowflake） |
| nickname | string | 昵称 |
| avatar | string | 头像 URL |

**登录错误码:**

| code | 含义 |
|------|------|
| 1001 | 邮箱为空 |
| 1002 | 邮箱或密码错误 |
| 1003 | 密码为空 |
| 1004 | 用户已封禁 |
| 1005 | 登录频率限制（防暴力破解） |

### 3.3 登出 (kLogout / kLogoutAck)

客户端主动登出，服务端清理会话状态并关闭连接。

**LogoutReq** (C→S, 0x0003):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| （body 可为空） | | | 使用帧头 uid 标识用户 |

**LogoutAck** (S→C, 0x0003) — 复用 RspBase:

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**服务端处理：**
1. 根据 `conn->user_id()` 获取当前连接用户
2. 清除 `conn` 上的 user_id / device_id
3. 从 `ConnManager` 移除该连接
4. 响应 LogoutAck(code=0)
5. 关闭 TCP 连接

**登出错误码：**

| code | 含义 |
|------|------|
| -2 | 未登录（当前连接未认证） |

---

### 3.4 搜索用户 (kSearchUser / kSearchUserAck)

按邮箱或昵称搜索其他用户，支持分页。

**SearchUserReq** (C→S, 0x0400):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| keyword | string | ✅ | 搜索关键词（含 `@` 按邮箱精确匹配，否则按昵称模糊搜索） |

**SearchUserAck** (S→C, 0x0401):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| users | SearchUserItem[] | 用户列表 |

**SearchUserItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| uid | string | 用户 UID（Snowflake） |
| nickname | string | 昵称 |
| avatar | string | 头像 URL |

**搜索规则：**
- `keyword` 包含 `@` 时按邮箱精确匹配（不区分大小写）
- 否则按昵称模糊匹配（LIKE `%keyword%`）
- 不返回自己、已封禁、已删除的用户
- 返回的邮箱做脱敏处理，仅展示首尾字符

**搜索错误码：**

| code | 含义 |
|------|------|
| -1 | 请求体格式无效 |
| -2 | 未登录 |
| 1017 | keyword 为空 |
| 1018 | keyword 超长 |

---

### 3.5 获取/修改个人资料 (kGetUserProfile / kUpdateProfile)

**GetUserProfileReq** (C→S, 0x0302):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| target_uid | string | | 目标用户 UID，空表示查自己 |

**GetUserProfileAck** (S→C, 0x0303):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| uid | string | 用户 UID（Snowflake） |
| nickname | string | 昵称 |
| avatar | string | 头像 URL |
| email | string | 邮箱（仅查自己时返回，查他人为空） |

**UpdateProfileReq** (C→S, 0x0402):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| nickname | string | | 新昵称（为空则不修改），1–100 字符 |
| avatar | string | | 新头像路径/URL（为空则不修改） |
| file_hash | string | | 头像文件哈希（仅当 avatar 非空时有意义） |

**UpdateProfileAck** (S→C, 0x0403):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

---

### 3.6 好友管理

#### 3.6.1 发送好友申请 (kAddFriend / kAddFriendAck)

**AddFriendReq** (C→S, 0x0030):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| target_uid | string | ✅ | 目标用户 UID（Snowflake） |
| remark | string | | 验证消息 / 备注（最长 200 字符） |

**AddFriendAck** (S→C, 0x0031):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=已发送申请 |
| msg | string | 错误描述 |
| request_id | int64 | 好友申请 ID（可用于跟踪状态） |

**服务端处理：**
1. 通过 `target_uid` 查找目标用户，校验非自己
2. 检查是否已是好友 / 是否已被拉黑 / 是否有未处理的申请
3. 写入 `friend_requests` 表（status=0 待确认）
4. 向对方推送 `kFriendNotify`（type=1 申请）

**错误码：**

| code | 含义 |
|------|------|
| 5001 | 不能添加自己 |
| 5002 | 已经是好友 |
| 5003 | 已有待处理的好友申请 |
| 5008 | 对方已拉黑你 |
| 5009 | target_uid 为空 |
| 1019 | 目标用户不存在 |

#### 3.6.2 处理好友申请 (kHandleFriendReq / kHandleFriendReqAck)

**HandleFriendReqReq** (C→S, 0x0032):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| request_id | int64 | ✅ | 好友申请 ID（来自 AddFriendAck 或 FriendNotify） |
| action | int32 | ✅ | 1=同意，2=拒绝 |

**HandleFriendReqAck** (S→C, 0x0033):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| conversation_id | int64 | 同意时返回私聊会话 ID（自动创建），拒绝时为 0 |

**服务端处理（同意）：**
1. 更新 `friend_requests` 记录 status=1（已通过）
2. 自动创建私聊 `Conversation`（type=Private），双方为成员
3. 双向写入 `friendships` 记录（status=1 正常）
4. 向申请发起方推送 `kFriendNotify`（type=2 已同意）

**错误码：**

| code | 含义 |
|------|------|
| 5004 | 申请不存在或已处理 |
| 5010 | request_id 无效 |
| 5011 | action 必须为 1 或 2 |

#### 3.6.3 删除好友 (kDeleteFriend / kDeleteFriendAck)

**DeleteFriendReq** (C→S, 0x0034):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| target_uid | string | ✅ | 要删除的好友 UID |

**DeleteFriendAck** (S→C, 0x0035):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**服务端处理：**
1. 更新双方 `friendships` 记录 status=3（已删除）
2. 保留私聊 Conversation 和历史消息（不删除）
3. 向对方推送 `kFriendNotify`（type=4 已删除）

**错误码：**

| code | 含义 |
|------|------|
| 5005 | 不是好友 |
| 5009 | target_uid 为空 |
| 1019 | 目标用户不存在 |

#### 3.6.4 拉黑/取消拉黑 (kBlockFriend / kUnblockFriend)

**BlockFriendReq** (C→S, 0x0036):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| target_uid | string | ✅ | 要拉黑的用户 UID |

**BlockFriendAck** (S→C, 0x0037):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**UnblockFriendReq** (C→S, 0x0038) / **UnblockFriendAck** (S→C, 0x0039)：结构相同。

**拉黑规则：**
- 拉黑后对方无法发送好友申请、无法发送消息
- 拉黑是单向的，不通知对方
- 取消拉黑后状态变为已删除（不自动恢复好友），对方可重新申请

**错误码：**

| code | 含义 |
|------|------|
| 5006 | 已经拉黑（Block） |
| 5007 | 未拉黑（Unblock） |
| 5009 | target_uid 为空 |
| 1019 | 目标用户不存在 |

#### 3.6.5 获取好友列表 (kGetFriendList / kGetFriendListAck)

**GetFriendListReq** (C→S, 0x003A):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| （body 可为空） | | | 返回全部好友 |

**GetFriendListAck** (S→C, 0x003B):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| friends | FriendItem[] | 好友列表 |

**FriendItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| uid | string | 好友 UID（Snowflake） |
| nickname | string | 昵称 |
| avatar | string | 头像 |
| conversation_id | int64 | 对应私聊会话 ID |

#### 3.6.6 获取好友申请列表 (kGetFriendRequests / kGetFriendRequestsAck)

**GetFriendRequestsReq** (C→S, 0x003C):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| page | int32 | | 页码，默认 1 |
| page_size | int32 | | 每页条数，默认 20 |

**GetFriendRequestsAck** (S→C, 0x003D):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| requests | FriendRequestItem[] | 申请列表 |
| total | int64 | 总数 |

**FriendRequestItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| request_id | int64 | 申请 ID |
| from_uid | string | 申请人 UID |
| from_nickname | string | 申请人昵称 |
| from_avatar | string | 申请人头像 |
| remark | string | 验证消息 |
| status | int32 | 0=待处理，1=已同意，2=已拒绝 |
| created_at | string | 申请时间 |

#### 3.6.7 好友变更推送 (kFriendNotify)

服务端主动推送，通知好友关系变化。

**FriendNotifyMsg** (S→C, 0x003E):

| 字段 | 类型 | 说明 |
|------|------|------|
| notify_type | int32 | 事件类型（见下） |
| from_uid | string | 相关用户 UID |
| from_nickname | string | 相关用户昵称 |
| from_avatar | string | 相关用户头像 |
| remark | string | 验证消息（仅申请时有值） |
| request_id | int64 | 申请 ID（申请/同意/拒绝时有值） |
| conversation_id | int64 | 私聊会话 ID（同意时有值） |

**notify_type 事件类型：**

| type | 含义 |
|------|------|
| 1 | 收到好友申请 |
| 2 | 对方已同意 |
| 3 | 对方已拒绝 |
| 4 | 被对方删除 |

---

### 3.7 消息

#### 3.7.1 消息类型 (msg_type)

| msg_type | 含义 | content 格式 |
|----------|------|-------------|
| 1 | 纯文本 | UTF-8 字符串，最长 5000 字符 |
| 2 | 表情 | 表情 ID 字符串，如 `"emoji:laugh"` 或 `"sticker:pack1/001"` |
| 3 | 图片 | JSON：`{"file_id":"...","width":640,"height":480,"thumb":"..."}` |
| 4 | 语音 | JSON：`{"file_id":"...","duration":15}` （秒） |
| 5 | 视频 | JSON：`{"file_id":"...","duration":30,"width":1280,"height":720,"thumb":"..."}` |
| 6 | 文件/文档 | JSON：`{"file_id":"...","file_name":"report.pdf","file_size":102400}` |
| 7 | 位置 | JSON：`{"lat":39.9,"lng":116.4,"name":"天安门","addr":"北京市东城区"}` |
| 8 | 名片 | JSON：`{"user_id":123,"nickname":"张三","avatar":"..."}` |
| 9 | 撤回通知 | JSON：`{"recalled_seq":42}` （系统消息，由服务端生成） |
| 10 | 系统通知 | UTF-8 系统消息文本（入群/退群/改名等） |
| 100+ | 自定义 | 客户端自行解析 |

> **大文件策略：** 图片/语音/视频/文件等富媒体消息需先通过文件上传接口（0x0500-0x0505）获取 `file_id`，再在消息 content 中引用。服务端不直接传输文件二进制数据，消息 body 始终 ≤ 1 MB。

#### 3.7.2 发送消息 (kSendMsg / kSendMsgAck)

**SendMsgReq** (C→S, 0x0100):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 目标会话 ID |
| content | string | ✅ | 消息内容（格式取决于 msg_type） |
| msg_type | int32 | ✅ | 消息类型，见 3.7.1 |
| client_msg_id | string | | 客户端幂等 ID（UUID），相同 ID 不重复入库 |

**SendMsgAck** (S→C, 0x0101):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| server_seq | int64 | 服务端分配的消息序号（会话内递增） |
| server_time | int64 | 服务端时间戳（epoch ms） |

**发送校验规则：**
- 发送者必须是该会话的成员
- 私聊：发送者未被对方拉黑
- 群聊：发送者未被禁言
- content 非空，长度 ≤ 消息 body 上限
- client_msg_id 幂等：相同 ID 返回之前的 server_seq，不重复入库

**发送错误码：**

| code | 含义 |
|------|------|
| 2001 | content 为空 |
| 2002 | content 超长 |
| 2003 | 无效的 conversation_id |
| 2004 | 会话不存在 |
| 2005 | 非会话成员 |

#### 3.7.3 消息推送 (kPushMsg)

**PushMsg** (S→C, 0x0102):

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 会话 ID |
| sender_uid | string | 发送者 UID（Snowflake） |
| content | string | 消息内容 |
| server_seq | int64 | 服务端消息序号 |
| server_time | int64 | 服务端时间戳（epoch ms） |
| msg_type | MsgType | 消息类型 |

**推送流程：**
1. 消息入库后，查询会话所有在线成员
2. 对每个在线成员（除发送者）推送 PushMsg
3. 等待客户端回 `kDeliverAck`
4. 离线成员在下次登录后通过 `kSyncUnread` 拉取

#### 3.7.4 送达确认 / 已读确认 (kDeliverAck / kReadAck)

**DeliverAckReq** (C→S, 0x0103):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 会话 ID |
| server_seq | int64 | ✅ | 已送达的消息 seq |

**ReadAckReq** (C→S, 0x0104):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 会话 ID |
| read_up_to_seq | int64 | ✅ | 已读到的最大 seq（批量标记） |

**服务端处理：**
- DeliverAck：更新 `conversation_members.last_ack_seq`
- ReadAck：更新 `conversation_members.last_read_seq`，重新计算未读数

#### 3.7.5 撤回消息 (kRecallMsg / kRecallMsgAck / kRecallNotify)

**RecallMsgReq** (C→S, 0x0105):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 会话 ID |
| server_seq | int64 | ✅ | 要撤回的消息 seq |

**RecallMsgAck** (S→C, 0x0106):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**RecallNotify** (S→C, 0x0107):

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 会话 ID |
| server_seq | int64 | 被撤回的消息 seq |
| operator_uid | string | 撤回操作者 UID（Snowflake） |

**撤回规则：**
- 仅消息发送者可撤回自己的消息
- 群管理员/群主撤回任意成员消息（待后续实现）
- 撤回时间限制：发送后 2 分钟内（可配置，`server.recall_timeout_secs`）
- 撤回后消息 status 置为 `Recalled`
- 对所有在线会话成员推送 `kRecallNotify`

**撤回错误码：**

| code | 含义 |
|------|------|
| 2005 | 非会话成员 |
| 2006 | 消息不存在 |
| 2007 | 超过撤回时间限制 |
| 2008 | 无权撤回该消息（非发送者） |
| 2009 | 消息已撤回 |

---

### 3.8 会话管理

会话（Conversation）是聊天的容器。私聊和群聊共用会话模型，通过 `type` 区分。

#### 3.8.1 创建会话 (kCreateConv / kCreateConvAck)

私聊会话在好友申请通过时自动创建，客户端通常不需要手动调用。群聊会话通过建群命令创建。

**CreateConvReq** (C→S, 0x0110):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | int32 | ✅ | 1=私聊，2=群聊 |
| target_user_id | int64 | 条件 | 私聊时必填，对方用户 ID |
| name | string | 条件 | 群聊时必填，群名称 |
| member_ids | int64[] | 条件 | 群聊时必填，初始成员 ID 列表 |

**CreateConvAck** (S→C, 0x0111):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| conversation_id | int64 | 新建的会话 ID |

**规则：**
- 私聊：两人之间最多一个会话，已存在则返回已有会话 ID
- 群聊：等同于 `kCreateGroup`（见 3.9）

#### 3.8.2 获取会话列表 (kGetConvList / kGetConvListAck)

**GetConvListReq** (C→S, 0x0112):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| page | int32 | | 页码，默认 1 |
| page_size | int32 | | 每页条数，默认 20，最大 100 |

**GetConvListAck** (S→C, 0x0113):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| conversations | ConvItem[] | 会话列表（按最新消息时间倒序） |
| total | int32 | 总数 |

**ConvItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 会话 ID |
| type | int32 | 1=私聊，2=群聊 |
| name | string | 会话名称（私聊为对方昵称，群聊为群名） |
| avatar | string | 会话头像（私聊为对方头像，群聊为群头像） |
| unread_count | int64 | 未读消息数 |
| last_msg | LastMsgBrief | 最后一条消息摘要 |
| mute | int32 | 0=正常，1=免打扰 |
| pinned | int32 | 0=不置顶，1=置顶 |
| updated_at | string | 最后更新时间 |

**LastMsgBrief 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| sender_id | int64 | 发送者 ID |
| sender_nickname | string | 发送者昵称 |
| content | string | 消息摘要（截断至 100 字符） |
| msg_type | int32 | 消息类型 |
| server_time | int64 | 时间戳 |

#### 3.8.3 删除会话 (kDeleteConv / kDeleteConvAck)

仅对当前用户隐藏会话，不删除消息数据。

**DeleteConvReq** (C→S, 0x0114):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 要删除（隐藏）的会话 ID |

**DeleteConvAck** (S→C, 0x0115):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**规则：** 隐藏后如收到新消息，会话自动恢复可见。

#### 3.8.4 免打扰 / 置顶 (kMuteConv / kPinConv)

**MuteConvReq** (C→S, 0x0116):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 会话 ID |
| mute | int32 | ✅ | 0=取消免打扰，1=开启免打扰 |

**PinConvReq** (C→S, 0x0118):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 会话 ID |
| pinned | int32 | ✅ | 0=取消置顶，1=置顶 |

两者的 Ack 均为 `RspBase`（code + msg）。

#### 3.8.5 会话变更推送 (kConvUpdate)

**ConvUpdate** (S→C, 0x011A):

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 会话 ID |
| type | int32 | 事件类型（见下） |
| data | string | JSON 附加数据 |

**type 事件类型：**

| type | 含义 | data |
|------|------|------|
| 1 | 新消息 | `{"sender_id":..., "content":"摘要", "msg_type":1}` |
| 2 | 成员变化 | `{"added":[id,...], "removed":[id,...]}` |
| 3 | 会话信息变更 | `{"name":"新群名", "avatar":"..."}` |
| 4 | 会话解散 | `{}` |

---

### 3.9 群组管理

#### 3.9.1 建群 (kCreateGroup / kCreateGroupAck)

**CreateGroupReq** (C→S, 0x0400):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| name | string | ✅ | 群名称，1–100 字符 |
| avatar | string | | 群头像 URL |
| member_ids | int64[] | ✅ | 初始成员 ID 列表（不含自己，最少 2 人） |

**CreateGroupAck** (S→C, 0x0401):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| conversation_id | int64 | 群会话 ID |
| group_id | int64 | 群 ID |

**服务端处理：**
1. 创建 `Conversation`（type=Group），创建者为 owner
2. 创建 `groups` 表记录
3. 将创建者 + member_ids 写入 `conversation_members`（创建者 role=Owner）
4. 对所有初始成员推送 `kGroupNotify`（type=创建）

**建群错误码：**

| code | 含义 |
|------|------|
| 1 | 群名为空或超长 |
| 2 | 初始成员不足 2 人 |
| 3 | 部分成员不存在 |
| 4 | 包含已拉黑你的用户 |

#### 3.9.2 解散群 (kDismissGroup / kDismissGroupAck)

**DismissGroupReq** (C→S, 0x0402):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |

**DismissGroupAck** (S→C, 0x0403):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**规则：** 仅群主可解散。解散后会话标记删除，对所有成员推送 `kGroupNotify`（type=解散）。

#### 3.9.3 申请入群 / 审批 (kJoinGroup / kHandleJoinReq)

**JoinGroupReq** (C→S, 0x0404):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |
| remark | string | | 申请备注 |

**JoinGroupAck** (S→C, 0x0405):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=已提交申请 |
| msg | string | 错误描述 |

**HandleJoinReqReq** (C→S, 0x0406):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |
| applicant_id | int64 | ✅ | 申请人用户 ID |
| action | int32 | ✅ | 1=同意，2=拒绝 |

**HandleJoinReqAck** (S→C, 0x0407):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**规则：** 仅群主/管理员可审批。同意后写入 `conversation_members`，推送 `kGroupNotify`。

#### 3.9.4 退群 (kLeaveGroup / kLeaveGroupAck)

**LeaveGroupReq** (C→S, 0x0408):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |

**LeaveGroupAck** (S→C, 0x0409):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**规则：** 群主不能退群（须先转让群主或解散群）。退出后从 `conversation_members` 移除，推送 `kGroupNotify`。

#### 3.9.5 踢出成员 (kKickMember / kKickMemberAck)

**KickMemberReq** (C→S, 0x040A):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |
| target_user_id | int64 | ✅ | 被踢用户 ID |

**KickMemberAck** (S→C, 0x040B):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**权限规则：**
- 群主可踢任何成员
- 管理员可踢普通成员
- 普通成员无踢出权限
- 不能踢自己（用退群）

#### 3.9.6 获取群信息 (kGetGroupInfo / kGetGroupInfoAck)

**GetGroupInfoReq** (C→S, 0x040C):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |

**GetGroupInfoAck** (S→C, 0x040D):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| conversation_id | int64 | 群会话 ID |
| name | string | 群名称 |
| avatar | string | 群头像 |
| owner_id | int64 | 群主用户 ID |
| notice | string | 群公告 |
| member_count | int32 | 群成员数 |
| created_at | string | 建群时间 |

#### 3.9.7 修改群信息 (kUpdateGroup / kUpdateGroupAck)

**UpdateGroupReq** (C→S, 0x040E):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |
| name | string | | 新群名（为空不修改），1–100 字符 |
| avatar | string | | 新头像（为空不修改） |
| notice | string | | 新公告（为空不修改），最长 1000 字符 |

**UpdateGroupAck** (S→C, 0x040F):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**权限：** 仅群主/管理员可修改。修改后对所有成员推送 `kGroupNotify`（type=信息变更）。

#### 3.9.8 获取群成员列表 (kGetGroupMembers / kGetGroupMembersAck)

**GetGroupMembersReq** (C→S, 0x0410):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |

**GetGroupMembersAck** (S→C, 0x0411):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| members | GroupMember[] | 成员列表 |

**GroupMember 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | int64 | 用户 ID |
| nickname | string | 昵称 |
| avatar | string | 头像 |
| role | int32 | 0=普通成员，1=管理员，2=群主 |
| joined_at | string | 入群时间 |

#### 3.9.9 获取我的群列表 (kGetMyGroups / kGetMyGroupsAck)

**GetMyGroupsReq** (C→S, 0x0412):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| （body 可为空） | | | 返回当前用户所有群 |

**GetMyGroupsAck** (S→C, 0x0413):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| groups | MyGroupItem[] | 群列表 |

**MyGroupItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 群会话 ID |
| name | string | 群名称 |
| avatar | string | 群头像 |
| member_count | int32 | 成员数 |
| my_role | int32 | 我的角色：0=成员，1=管理员，2=群主 |

#### 3.9.10 设置成员角色 (kSetMemberRole / kSetMemberRoleAck)

**SetMemberRoleReq** (C→S, 0x0414):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 群会话 ID |
| target_user_id | int64 | ✅ | 目标成员 ID |
| role | int32 | ✅ | 0=普通成员，1=管理员 |

**SetMemberRoleAck** (S→C, 0x0415):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |

**权限：** 仅群主可设置/撤销管理员。

#### 3.9.11 群事件推送 (kGroupNotify)

**GroupNotify** (S→C, 0x0416):

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 群会话 ID |
| type | int32 | 事件类型（见下） |
| operator_id | int64 | 操作者用户 ID |
| target_ids | int64[] | 涉及的用户 ID 列表 |
| data | string | JSON 附加数据 |

**type 事件类型：**

| type | 含义 | data |
|------|------|------|
| 1 | 群创建 | `{"name":"群名"}` |
| 2 | 群解散 | `{}` |
| 3 | 成员加入 | `{"nicknames":["张三"]}` |
| 4 | 成员退出 | `{}` |
| 5 | 成员被踢 | `{}` |
| 6 | 群信息变更 | `{"name":"新群名","notice":"新公告"}` |
| 7 | 角色变更 | `{"role":1}` |
| 8 | 入群申请 | `{"remark":"申请备注"}` |

---

### 3.10 文件上传/下载

富媒体消息（图片/语音/视频/文件）的传输分为两步：先上传文件获取 `file_id`，再在消息中引用。

#### 3.10.1 请求上传 (kUploadReq / kUploadAck)

**UploadReq** (C→S, 0x0500):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file_name | string | ✅ | 原始文件名 |
| file_size | int64 | ✅ | 文件大小（字节） |
| mime_type | string | ✅ | MIME 类型，如 `image/png` |
| file_hash | string | | 文件 SHA-256（秒传去重） |
| file_type | string | | 用途分类：`avatar`/`image`/`voice`/`video`/`file` |

**UploadAck** (S→C, 0x0501):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| file_id | string | 文件唯一标识 |
| upload_url | string | HTTP PUT 上传地址（有时效性） |
| already_exists | int32 | 1=秒传命中（file_hash 已存在），无需上传 |

**文件大小限制：**

| file_type | 最大大小 |
|-----------|---------|
| avatar | 2 MB |
| image | 10 MB |
| voice | 5 MB |
| video | 100 MB |
| file | 100 MB |

> **大文件上传：** 超过 10 MB 的文件建议使用分片上传。客户端可将文件切分为 2 MB 分片，逐片 PUT 到 `upload_url/{chunk_index}`。详见后续分片协议扩展。

#### 3.10.2 上传完成通知 (kUploadComplete / kUploadCompleteAck)

客户端完成 HTTP PUT 后通知服务端确认。

**UploadCompleteReq** (C→S, 0x0502):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file_id | string | ✅ | 文件 ID（来自 UploadAck） |

**UploadCompleteAck** (S→C, 0x0503):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| file_path | string | 文件可访问路径/URL |

#### 3.10.3 请求下载 (kDownloadReq / kDownloadAck)

**DownloadReq** (C→S, 0x0504):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file_id | string | ✅ | 文件 ID |
| thumb | int32 | | 1=仅缩略图（图片/视频） |

**DownloadAck** (S→C, 0x0505):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| msg | string | 错误描述 |
| download_url | string | HTTP GET 下载地址（有时效性） |
| file_name | string | 文件名 |
| file_size | int64 | 文件大小 |

---

### 3.11 同步

#### 3.11.1 拉取历史消息 (kSyncMsg / kSyncMsgResp)

**SyncMsgReq** (C→S, 0x0200):

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| conversation_id | int64 | ✅ | 会话 ID |
| last_seq | int64 | ✅ | 客户端已有的最大 seq（从该 seq 之后拉取） |
| limit | int32 | | 拉取条数，默认 20，最大 100 |

**SyncMsgResp** (S→C, 0x0201):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| messages | SyncMsgItem[] | 消息列表（按 seq 升序） |
| has_more | bool | 是否还有更多消息 |

**SyncMsgItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| server_seq | int64 | 消息序号 |
| sender_uid | string | 发送者 UID（Snowflake） |
| content | string | 消息内容 |
| msg_type | MsgType | 消息类型 |
| server_time | string | 发送时间 |
| status | MsgStatus | 0=正常，1=已撤回，2=已删除 |

#### 3.11.2 拉取未读 (kSyncUnread / kSyncUnreadResp)

**SyncUnreadReq** (C→S, 0x0202): body 可为空。

**SyncUnreadResp** (S→C, 0x0203):

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int32 | 0=成功 |
| items | UnreadItem[] | 有未读消息的会话列表 |
| total_unread | int64 | 全部未读消息总数 |

**UnreadItem 结构：**

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | int64 | 会话 ID |
| count | int64 | 该会话未读数 |
| latest_messages | SyncMsgItem[] | 最近几条消息摘要 |

---

## 4. 通用错误码（TCP 协议）

| code | 含义 | 说明 |
|------|------|------|
| 0 | 成功 | 所有命令通用 |
| -1 | 请求体格式无效 | struct_pack 反序列化失败 |
| -2 | 未登录 | 需要认证但未登录 |
| -100 | 数据库错误 | 服务端内部错误 |
| -503 | 服务繁忙 | 服务端过载 |

> 负数为通用系统级错误码；正数为各业务命令的专属错误码（见各命令定义）。

---

## 5. Admin HTTP API 协议

独立端口 (默认 9091)，RESTful JSON API。

### 响应格式

```json
{
  "code": 0,
  "msg": "ok",
  "data": { ... }
}
```

### 错误码

| code | 含义 | HTTP 状态码 |
|------|------|----------|
| 0 | 成功 | 200 |
| 1 | 参数错误 | 200/400/409/413 |
| 2 | 未登录 | 401 |
| 3 | 无权限 | 403/429 |
| 4 | 资源不存在 | 200 |
| 5 | 内部错误 | 200 |

> 所有错误响应均已提取为 `api_err` 命名空间的 32 个 `constexpr ApiError` 常量，
> 定义在 `server/admin/http_helper.h` 中，消除了所有 hardcode 字符串。

### 鉴权

- `Authorization: Bearer <JWT>` (HS256)
- JWT payload: `{sub: "admin_id", iss: "nova", iat: ..., exp: ...}`
- 免鉴权路径: `/healthz`, `/api/v1/auth/login`

### 分页

请求: `?page=1&page_size=20` (默认 page=1, page_size=20, 最大 100)

响应:
```json
{
  "items": [...],
  "total": 156,
  "page": 1,
  "page_size": 20
}
```

详细 API 设计见 [admin_server/api_design.md](admin_server/api_design.md)。