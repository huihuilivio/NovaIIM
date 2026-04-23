# NovaIIM 子功能时序图

本文档汇总各核心子功能在客户端、Server、数据库与第三方组件之间的交互时序，便于排查链路与对照实现。

参与方说明：
- **C** Client（nova_sdk + 平台 UI）
- **GW** Gateway / TCP server
- **Svc** Service 层（UserSvc / MsgSvc / FriendSvc / GroupSvc / ConvSvc / SyncSvc / FileSvc）
- **DAO** ormpp DAO + DbManager
- **DB** SQLite / MySQL
- **CM** ConnManager（多端连接）
- **Bus** 内部 MsgBus（事件解耦）
- **Admin** Admin Web 前端
- **AS** AdminServer（HTTP :9091）
- **FS** FileServer（HTTP :9092）

---

## 1. 注册

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW as Gateway
    participant Svc as UserSvc
    participant DAO
    participant DB

    C->>GW: kRegister (email, nickname, password)
    GW->>Svc: HandleRegister(pkt)
    Svc->>Svc: trim + lowercase email<br/>校验格式 / 密码长度
    Svc->>DAO: User.FindByEmail(email)
    DAO->>DB: SELECT
    DB-->>DAO: 空 / 已存在
    alt 已存在
        Svc-->>C: kRegisterAck { code=EmailUsed }
    else 新邮箱
        Svc->>Svc: PBKDF2-SHA256(password, salt, 100k)
        Svc->>Svc: Snowflake.Next() → uid
        Svc->>DAO: User.Insert(uid, email, hash, salt, nickname)
        DAO->>DB: INSERT OR IGNORE
        DB-->>DAO: rows
        alt rows == 0（TOCTOU）
            Svc-->>C: kRegisterAck { code=EmailUsed }
        else
            Svc-->>C: kRegisterAck { code=0, uid }
        end
    end
```

---

## 2. 登录

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW as Gateway
    participant Svc as UserSvc
    participant DAO
    participant DB
    participant CM as ConnManager

    C->>GW: kLogin (email, password, device_id, device_type)
    GW->>Svc: HandleLogin(conn, pkt)
    Svc->>Svc: RateLimiter.Check(IP)
    alt 频控触发
        Svc-->>C: kLoginAck { code=RateLimited }
    else 正常
        Svc->>DAO: User.FindByEmail(trim+lower)
        DAO->>DB: SELECT
        DB-->>DAO: User?
        alt 用户不存在 / 已封禁
            Svc-->>C: kLoginAck { code=AuthFailed }
        else
            Svc->>Svc: PBKDF2 验证 + 常量时间比较
            Svc->>Svc: memset_volatile(password)
            alt 密码错误
                Svc-->>C: kLoginAck { code=AuthFailed }
            else
                Svc->>CM: Add(uid, conn, device_id)
                Svc->>DAO: Device.Upsert(uid, device_id, last_seen)
                Svc-->>C: kLoginAck { code=0, uid, nickname }
            end
        end
    end
```

---

## 3. 心跳

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW as Gateway
    participant Svc as UserSvc
    participant CM as ConnManager

    loop 每 heartbeat_interval_ms
        C->>GW: kHeartbeat
        GW->>Svc: HandleHeartbeat(conn)
        Note right of Svc: 仅信任 conn->user_id()<br/>忽略 pkt.uid
        Svc->>CM: Refresh(uid, device_id)
        Svc-->>C: kHeartbeatAck
    end

    Note over GW: 超时未收到心跳<br/>libhv ReadTimeout → OnDisconnect
    GW->>CM: Remove(uid, conn)
```

---

## 4. 自动重连（客户端）

```mermaid
sequenceDiagram
    autonumber
    participant App as 客户端业务
    participant Tcp as TcpClient
    participant RM as ReconnectManager
    participant Timer

    Tcp->>App: OnStateChanged(Disconnected)
    App->>RM: OnStateChanged(Disconnected)
    RM->>RM: delay = min(prev*2, max_delay)<br/>delay += jitter
    RM->>Timer: SetTimeout(delay)
    Timer-->>RM: fire
    RM->>Tcp: Connect(host, port)
    alt 成功
        Tcp->>App: OnStateChanged(Connected)
        App->>RM: OnStateChanged(Connected)
        RM->>RM: 重置 delay = min_delay
    else 失败 / 超时
        Tcp->>App: OnStateChanged(Disconnected)
        Note over RM: 进入下一次退避
    end
```

---

## 5. 发送私聊消息（在线接收）

```mermaid
sequenceDiagram
    autonumber
    participant A as Client A
    participant GW as Gateway
    participant Msg as MsgSvc
    participant Conv as ConvSvc
    participant DAO
    participant DB
    participant CM as ConnManager
    participant B as Client B

    A->>GW: kSendMsg (conv_id, client_msg_id, content, msg_type)
    GW->>Msg: HandleSendMsg(conn, pkt)
    Msg->>Msg: dedup.CheckOrInsert(client_msg_id)
    alt 重复
        Msg-->>A: kSendMsgAck { 旧 server_seq }
    else 新消息
        Msg->>Conv: NextSeq(conv_id)
        Conv->>DAO: Conversation.IncrMaxSeq(conv_id)
        DAO->>DB: UPDATE ... RETURNING max_seq
        DB-->>Conv: server_seq
        Msg->>DAO: Message.Insert(server_seq, sender, conv_id, content)
        DAO->>DB: INSERT
        Msg->>Conv: UpdateLastMsg(conv_id, summary, time)
        Msg-->>A: kSendMsgAck { code=0, server_seq, server_time }
        Msg->>CM: GetConns(B.uid)
        loop B 的每个在线设备
            Msg->>B: kPushMsg (conv_id, sender, server_seq, content)
        end
        Msg->>CM: GetConns(A.uid) - {当前 conn}
        loop A 其他端
            Msg->>A: kPushMsg (多端同步)
        end
    end
```

---

## 6. 离线消息同步

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW as Gateway
    participant Sync as SyncSvc
    participant DAO
    participant DB

    Note over C: 上线 / 本地 last_seq 落后
    C->>GW: kSyncMsg (conv_id, last_seq, limit)
    GW->>Sync: HandleSyncMsg
    Sync->>DAO: Message.GetAfterSeq(conv_id, last_seq, limit)
    DAO->>DB: SELECT WHERE seq>? ORDER BY seq LIMIT ?
    DB-->>DAO: rows
    Sync-->>C: kSyncMsgAck { messages[], has_more }
    opt has_more
        C->>GW: kSyncMsg (conv_id, max(seq), limit)
    end
```

---

## 7. 消息送达 / 已读

```mermaid
sequenceDiagram
    autonumber
    participant B as Receiver
    participant GW as Gateway
    participant Msg as MsgSvc
    participant DAO
    participant CM as ConnManager
    participant A as Sender

    B->>GW: kDeliverAck (conv_id, server_seq)
    GW->>Msg: HandleDeliverAck
    Msg->>DAO: Message.UpdateStatus(seq, Delivered)
    Msg->>CM: GetConns(A.uid)
    Msg-->>A: kDeliverNotify

    B->>GW: kReadAck (conv_id, read_up_to_seq)
    GW->>Msg: HandleReadAck
    Msg->>DAO: ConvMember.UpdateReadSeq(conv_id, B.uid, seq)
    Msg-->>A: kReadNotify
```

---

## 8. 消息撤回

```mermaid
sequenceDiagram
    autonumber
    participant A as 发送方
    participant GW as Gateway
    participant Msg as MsgSvc
    participant DAO
    participant CM
    participant Peers as 会话其他成员

    A->>GW: kRecallMsg (conv_id, server_seq)
    GW->>Msg: HandleRecall
    Msg->>DAO: Message.FindBySeq(conv_id, seq)
    alt 非本人 / 超时（>2min）
        Msg-->>A: kRecallAck { code=Forbidden }
    else
        Msg->>DAO: Message.MarkRecalled(seq)
        Msg-->>A: kRecallAck { code=0 }
        Msg->>CM: 广播 kRecallNotify
        CM-->>Peers: kRecallNotify (conv_id, seq, operator)
    end
```

---

## 9. 添加好友

```mermaid
sequenceDiagram
    autonumber
    participant A
    participant GW as Gateway
    participant Friend as FriendSvc
    participant DAO
    participant CM
    participant B

    A->>GW: kAddFriend (target_uid, remark)
    GW->>Friend: HandleAddFriend
    Friend->>DAO: User.FindById(target)
    Friend->>DAO: Friendship.FindByUsers(A, B)
    alt 已为好友 / 已被拉黑
        Friend-->>A: kAddFriendAck { code=... }
    else
        Friend->>DAO: Friendship.Insert(pending request)
        Friend-->>A: kAddFriendAck { code=0, request_id }
        Friend->>CM: GetConns(B.uid)
        Friend->>B: FriendNotify { type=NewRequest, from=A }
    end
```

---

## 10. 处理好友申请（同意自动建私聊）

```mermaid
sequenceDiagram
    autonumber
    participant B as 申请接收方
    participant GW as Gateway
    participant Friend as FriendSvc
    participant Conv as ConvSvc
    participant DAO
    participant CM
    participant A as 申请发起方

    B->>GW: kHandleFriendReq (request_id, action=Accept|Reject)
    GW->>Friend: HandleFriendReq
    Friend->>DAO: Friendship.FindById(request_id)
    Note right of Friend: 校验当前 uid == receiver
    alt Reject
        Friend->>DAO: Friendship.UpdateStatus(Rejected)
    else Accept
        Friend->>DAO: Friendship.UpdateStatus(Accepted, 双向)
        Friend->>Conv: GetOrCreatePrivate(A, B)
        Conv->>DAO: Conversation.FindByUsers / Insert
        Conv->>DAO: ConvMember.Add(A,B)
    end
    Friend-->>B: kHandleFriendReqAck { conversation_id? }
    Friend->>CM: GetConns(A)
    Friend->>A: FriendNotify { type=Accepted, conv_id }
```

---

## 11. 创建群组

```mermaid
sequenceDiagram
    autonumber
    participant Owner
    participant GW
    participant Group as GroupSvc
    participant Conv as ConvSvc
    participant DAO
    participant CM
    participant Members

    Owner->>GW: kCreateGroup (name, avatar, member_ids)
    GW->>Group: HandleCreateGroup
    Group->>Group: 校验 size<=500 + 成员状态
    Group->>DAO: User.FindByIds(member_ids) 批量
    Group->>Conv: CreateGroupConversation()
    Conv->>DAO: Conversation.Insert(type=Group)
    Conv->>DAO: ConvMember.AddBatch(owner+members)
    Group->>DAO: Group.Insert(conv_id, owner, name, avatar)
    Group-->>Owner: kCreateGroupAck { code=0, conversation_id }
    Group->>CM: 广播给所有成员
    CM-->>Members: GroupNotify { type=Joined, conv_id }
```

---

## 12. 文件上传（小文件，HTTP）

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW as Gateway (TCP)
    participant FileSvc
    participant DAO
    participant FS as FileServer (HTTP :9092)
    participant Disk as 本地存储

    C->>GW: kUploadReq (filename, size, hash)
    GW->>FileSvc: HandleUploadReq
    FileSvc->>DAO: UserFile.FindByHash(hash)
    alt 命中（秒传）
        FileSvc-->>C: kUploadReqAck { file_id, instant=true }
    else
        FileSvc->>DAO: UserFile.Insert(status=Uploading)
        FileSvc-->>C: kUploadReqAck { file_id, upload_url, token }
        C->>FS: POST /api/v1/files/upload (multipart)
        FS->>FS: IsPathSafe(filename) + Content-Length 校验
        FS->>Disk: 写入 root_dir/{stored_name}
        FS-->>C: 200 { url }
        C->>GW: kUploadComplete (file_id)
        GW->>FileSvc: HandleUploadComplete
        FileSvc->>DAO: UserFile.UpdateStatus(Done, url)
        FileSvc-->>C: kUploadCompleteAck { code=0 }
    end
```

---

## 13. 文件下载

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW
    participant FileSvc
    participant DAO
    participant FS as FileServer

    C->>GW: kDownloadReq (file_id)
    GW->>FileSvc: HandleDownloadReq
    FileSvc->>DAO: UserFile.FindById + 共享会话鉴权
    alt 无权限
        FileSvc-->>C: kDownloadReqAck { code=Forbidden }
    else
        FileSvc-->>C: kDownloadReqAck { url, expires_at }
        C->>FS: GET /static/{stored_name}
        FS-->>C: 文件内容（小文件走 FileCache，大文件流式）
    end
```

---

## 14. Admin 登录 + 受保护接口

```mermaid
sequenceDiagram
    autonumber
    participant Admin as Admin Web
    participant AS as AdminServer
    participant DAO
    participant DB

    Admin->>AS: POST /api/v1/auth/login (uid, password)
    AS->>AS: RateLimiter（5/60s/IP）
    AS->>DAO: Admin.FindByUid + status!=deleted
    DAO->>DB: SELECT
    AS->>AS: PBKDF2 校验
    AS->>DAO: AdminSession.Insert(jti, exp)
    AS-->>Admin: { token, expires_in }

    Admin->>AS: GET /api/v1/users<br/>Authorization: Bearer token
    AS->>AS: 清除伪造的 X-Nova-* 请求头
    AS->>AS: JWT 验签 + jti 黑名单
    AS->>DAO: AdminRole.GetPermissions(admin_id)
    AS->>AS: RequirePermission("user.view")
    AS->>DAO: User.List(page, filter)
    AS->>DAO: AuditLog.Insert(admin_id, action, IP)
    AS-->>Admin: { code=0, data: [...] }
```

---

## 15. Admin 踢人下线

```mermaid
sequenceDiagram
    autonumber
    participant Admin as Admin Web
    participant AS as AdminServer
    participant Bus as MsgBus
    participant CM as ConnManager
    participant Target as 目标客户端

    Admin->>AS: POST /api/v1/users/:id/kick
    AS->>AS: RequirePermission("user.kick")
    AS->>Bus: publish("user.kick", {uid, reason})
    Bus-->>CM: 订阅回调
    CM->>CM: GetConns(uid)
    loop 每个连接
        CM->>Target: kKickNotify (reason)
        CM->>Target: Close()
    end
    AS-->>Admin: { code=0 }
    Note over Target: 客户端收到 kKickNotify<br/>停止 ReconnectManager → kDisconnected
```

---

## 16. JS Bridge 端到端（桌面端单条消息发送）

```mermaid
sequenceDiagram
    autonumber
    participant UI as Vue 3
    participant TS as NovaBridge (TS)
    participant JB as JsBridge (C++)
    participant VM as ChatVM
    participant Svc as MessageService
    participant Net as TcpClient
    participant Server

    UI->>TS: send("sendMessage", {to, content})
    TS->>JB: chrome.webview.postMessage(json)
    JB->>JB: parse + action 分发
    JB->>VM: SendTextMessage(conv_id, content, cb)
    VM->>Svc: SendText(...)
    Svc->>Net: kSendMsg packet
    Net->>Server: TCP write
    Server-->>Net: kSendMsgAck
    Net-->>Svc: response (RequestManager 匹配 seq)
    Svc-->>VM: cb(SendMsgResult)
    VM->>JB: PostEvent("sendMsgResult", json)
    JB->>JB: UIDispatcher::Post → UI 线程
    JB->>TS: window.__novaBridge.onEvent(json)
    TS->>UI: emit "sendMsgResult"
```

---

## 17. 客户端缓存写入与读取

```mermaid
sequenceDiagram
    autonumber
    participant Net as 网络回调
    participant VM as ChatVM
    participant Cache as MessageCacheDao
    participant SQLite

    Note over Net: 收到 kPushMsg / SyncMsg 批量
    Net->>VM: OnReceived(messages[])
    VM->>Cache: InsertBatch(msgs)
    Cache->>SQLite: BEGIN
    loop 每条
        Cache->>SQLite: INSERT OR IGNORE
    end
    alt 任一失败
        Cache->>SQLite: ROLLBACK
        Note right of Cache: 整批回退，记录日志
    else 全部成功
        Cache->>SQLite: COMMIT
    end

    Note over VM: 用户进入会话
    VM->>Cache: GetByConversation(conv_id, limit, before_seq)
    Cache->>SQLite: SELECT ... ORDER BY server_seq DESC
    SQLite-->>VM: rows
```

---

## 18. NovaClient 启动 / 关闭

```mermaid
sequenceDiagram
    autonumber
    participant App as 平台入口
    participant NC as NovaClient
    participant Ctx as ClientContext
    participant Net as TcpClient
    participant Bus as MsgBus

    App->>NC: ctor(config_path)
    App->>NC: Init()  （幂等）
    NC->>Ctx: Init()
    Ctx->>Bus: start()
    Ctx->>Net: 创建 + 拆包配置
    Ctx->>Ctx: SetupPacketDispatch + StartTimeoutChecker
    NC->>NC: CreateVMs() + 注册 OnStateChanged

    App->>NC: Connect()
    NC->>Net: Connect(host, port)
    Net-->>Ctx: OnStateChanged(Connected → Authenticated)
    Ctx->>NC: state==Authenticated
    NC->>NC: OpenCache(uid) + 自动同步会话/好友/未读

    Note over App: 退出
    App->>NC: Shutdown() （幂等）
    NC->>Ctx: CloseCache + Shutdown
    Ctx->>Net: Disconnect
    Ctx->>Ctx: CancelAll pending requests
    Ctx->>Bus: stop()
```

---

## 19. IM Server 启动（依赖注入 + 路由注册 + 监听）

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Cfg as ConfigLoader
    participant DI as ServiceContainer
    participant DAO
    participant DB
    participant Bus as MsgBus
    participant GW as Gateway
    participant AS as AdminServer
    participant FS as FileServer

    Main->>Cfg: Load(configs/server.yaml)
    Cfg-->>Main: ServerConfig
    Main->>DI: Register<DbManager>(cfg.db)
    Main->>DAO: Init(connection_pool)
    DAO->>DB: 建池 + ping
    Main->>DI: Register<MsgBus>() + start()
    Main->>DI: Register<ConnManager / Snowflake / RateLimiter>
    Main->>DI: Register<UserSvc / FriendSvc / GroupSvc / ConvSvc / MsgSvc / SyncSvc / FileSvc>
    Main->>GW: Init(cfg.tcp_port) + RegisterHandlers(cmd→Svc)
    Main->>AS: Init(cfg.admin_port) + RegisterRoutes
    Main->>FS: Init(cfg.file_port) + 静态目录
    Main->>GW: Start()
    Main->>AS: Start()
    Main->>FS: Start()
    Main->>Main: SignalHandler(SIGINT/SIGTERM)
```

---

## 20. IM Server 优雅关闭

```mermaid
sequenceDiagram
    autonumber
    participant OS
    participant Main
    participant GW as Gateway
    participant CM as ConnManager
    participant AS as AdminServer
    participant FS as FileServer
    participant Bus as MsgBus
    participant DAO
    participant DB

    OS->>Main: SIGINT / SIGTERM
    Main->>GW: Stop accept()
    Main->>AS: Stop()
    Main->>FS: Stop()
    Main->>CM: BroadcastShutdownNotify
    loop 在线连接
        CM-->>CM: kServerShutdown + Close(graceful)
    end
    Main->>Bus: drain + stop
    Main->>DAO: FlushPendingWrites
    Main->>DB: Close pool
    Main->>Main: 退出码 0
```

---

## 21. Gateway 收包流程（拆包→限流→鉴权→分发）

```mermaid
sequenceDiagram
    autonumber
    participant Sock as TCP Socket
    participant Codec as PacketCodec
    participant GW as Gateway
    participant RL as RateLimiter
    participant Disp as Handler 路由表
    participant Svc as 目标 Service

    Sock->>Codec: bytes
    Codec->>Codec: 校验 magic + 长度<br/>解析 Header(cmd, seq, uid?)
    alt 包错误
        Codec->>GW: ProtocolError → Close
    else
        Codec->>GW: Packet
        GW->>RL: Check(uid 或 IP, cmd)
        alt 触发限流
            GW-->>Sock: ErrorAck { code=RateLimited }
        else
            GW->>GW: 是否需要鉴权？
            alt 需要 && conn.user_id() == 0
                GW-->>Sock: ErrorAck { code=Unauthorized }
            else
                GW->>Disp: Lookup(cmd)
                Disp->>Svc: Handle(conn, pkt)
                Svc-->>Sock: Ack / Push
            end
        end
    end
```

---

## 22. 多端互踢（同账号新设备登录）

```mermaid
sequenceDiagram
    autonumber
    participant New as 新设备
    participant GW as Gateway
    participant Usr as UserSvc
    participant CM as ConnManager
    participant Old as 旧设备

    New->>GW: kLogin (device_id=new)
    GW->>Usr: HandleLogin
    Usr->>Usr: 校验通过
    Usr->>CM: GetConnByDevice(uid, device_type, device_id)
    alt 同 device_type 已有连接 (single-instance per type)
        CM-->>Usr: oldConn
        Usr->>Old: kKickNotify { reason=AnotherDeviceLogin }
        Usr->>CM: Remove(oldConn) + Close
    end
    Usr->>CM: Add(uid, newConn, new device)
    Usr-->>New: kLoginAck { code=0 }
```

---

## 23. 客户端修改密码

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant GW
    participant Usr as UserSvc
    participant DAO
    participant CM as ConnManager
    participant Other as 同账号其他设备

    C->>GW: kChangePassword (old_pwd, new_pwd)
    GW->>Usr: HandleChangePassword
    Usr->>DAO: User.FindById(uid)
    Usr->>Usr: PBKDF2 校验 old_pwd
    alt 旧密码错误
        Usr-->>C: kChangePasswordAck { code=AuthFailed }
    else
        Usr->>Usr: 校验 new_pwd 强度<br/>派生新 hash + 新 salt
        Usr->>DAO: User.UpdatePassword(uid, hash, salt)
        Usr-->>C: kChangePasswordAck { code=0 }
        Note over Usr: 安全策略：踢出其他端
        Usr->>CM: GetConns(uid) - {当前 conn}
        loop 其他端
            Usr->>Other: kKickNotify { reason=PasswordChanged }
        end
    end
```

---

## 24. AdminServer 启动 + 中间件链装配

```mermaid
sequenceDiagram
    autonumber
    participant Main
    participant AS as AdminServer
    participant Hv as libhv HttpServer
    participant Mw as MiddlewareChain

    Main->>AS: Init(port, jwt_secret, cors_cfg)
    AS->>Mw: Use(CORS)
    AS->>Mw: Use(SecurityHeaders)
    AS->>Mw: Use(RequestId + AccessLog)
    AS->>Mw: Use(RateLimit)
    AS->>Mw: Use(StripUntrustedHeaders) — 移除伪造的 X-Nova-*
    AS->>Mw: Use(JwtAuth)（白名单：login / health）
    AS->>Mw: Use(Permission)
    AS->>AS: Routes.Register(/api/v1/*)
    AS->>Hv: Listen(port)
    AS-->>Main: Started
```

---

## 25. AdminServer 受保护请求中间件链路

```mermaid
sequenceDiagram
    autonumber
    participant Admin as Admin Web
    participant Hv as libhv
    participant CORS
    participant RL as RateLimit
    participant Strip as StripHeaders
    participant Jwt as JwtAuth
    participant Perm as Permission
    participant H as Handler
    participant Audit as AuditLog

    Admin->>Hv: HTTP 请求 (Authorization: Bearer ...)
    Hv->>CORS: 处理 OPTIONS / 写 CORS 头
    CORS->>RL: next
    RL->>RL: Check(IP + path)
    alt 触发
        RL-->>Admin: 429 Too Many Requests
    else
        RL->>Strip: next
        Strip->>Strip: 删除 X-Nova-Admin-Id 等伪造头
        Strip->>Jwt: next
        Jwt->>Jwt: 解析 token + 验签 + exp + jti 黑名单
        alt 无效
            Jwt-->>Admin: 401 Unauthorized
        else
            Jwt->>Jwt: ctx.admin_id = claims.sub
            Jwt->>Perm: next
            Perm->>Perm: 反射路由所需权限码
            Perm->>Perm: 查询 admin_id 的角色权限集
            alt 无权限
                Perm-->>Admin: 403 Forbidden
            else
                Perm->>H: next
                H->>H: 业务处理
                H->>Audit: Insert(admin_id, action, target, IP, UA)
                H-->>Admin: { code=0, data }
            end
        end
    end
```

---

## 26. Admin Token 续期 / 登出（jti 黑名单）

```mermaid
sequenceDiagram
    autonumber
    participant Admin
    participant AS as AdminServer
    participant DAO

    Admin->>AS: POST /api/v1/auth/refresh<br/>Authorization: Bearer old_token
    AS->>AS: JwtAuth 校验 old_token
    AS->>DAO: AdminSession.IsRevoked(old_jti)?
    alt 已撤销
        AS-->>Admin: 401
    else
        AS->>DAO: AdminSession.Insert(new_jti, exp)
        AS->>DAO: AdminSession.Revoke(old_jti)
        AS-->>Admin: { token: new, expires_in }
    end

    Note over Admin: 用户登出
    Admin->>AS: POST /api/v1/auth/logout
    AS->>DAO: AdminSession.Revoke(jti)
    AS-->>Admin: { code=0 }
    Note right of AS: 后续携带该 jti 的请求<br/>会被 JwtAuth 拒绝
```

---

## 27. Admin RBAC 角色授予

```mermaid
sequenceDiagram
    autonumber
    participant Super as 超级管理员
    participant AS as AdminServer
    participant DAO
    participant DB
    participant Audit

    Super->>AS: POST /api/v1/admins/:id/roles<br/>{ role_ids: [...] }
    AS->>AS: RequirePermission("admin.role.assign")
    AS->>DAO: Role.FindByIds(role_ids)
    alt 含未知角色 / 含 superadmin 但当前非 super
        AS-->>Super: 400 / 403
    else
        AS->>DB: BEGIN
        AS->>DAO: AdminRole.DeleteByAdmin(id)
        AS->>DAO: AdminRole.InsertBatch(id, role_ids)
        AS->>DB: COMMIT
        AS->>Audit: log("role.assign", target=id, diff)
        AS-->>Super: { code=0 }
    end
```

---

## 28. Admin 用户封禁 / 解封

```mermaid
sequenceDiagram
    autonumber
    participant Admin
    participant AS as AdminServer
    participant DAO
    participant Bus as MsgBus
    participant CM as ConnManager
    participant Target as 目标用户连接

    Admin->>AS: POST /api/v1/users/:uid/ban<br/>{ reason, until }
    AS->>AS: RequirePermission("user.ban")
    AS->>DAO: User.UpdateStatus(uid, Banned, until)
    AS->>Bus: publish("user.kick", {uid, reason="banned"})
    Bus-->>CM: 订阅回调
    CM->>Target: kKickNotify + Close
    AS->>DAO: AuditLog.Insert(...)
    AS-->>Admin: { code=0 }

    Note over AS: 解封路径
    Admin->>AS: POST /api/v1/users/:uid/unban
    AS->>DAO: User.UpdateStatus(uid, Active, NULL)
    AS->>DAO: AuditLog.Insert(...)
    AS-->>Admin: { code=0 }
```

---

## 29. Admin 审计日志查询

```mermaid
sequenceDiagram
    autonumber
    participant Admin
    participant AS as AdminServer
    participant DAO
    participant DB

    Admin->>AS: GET /api/v1/audit-logs?admin_id&action&from&to&page
    AS->>AS: RequirePermission("audit.view")
    AS->>AS: 校验时间范围 ≤ 90d，page ≤ MaxPage
    AS->>DAO: AuditLog.Query(filter, page, size)
    DAO->>DB: SELECT ... WHERE ...
    DB-->>DAO: rows + total
    AS-->>Admin: { code=0, data: [...], total }
```

---

## 30. Admin 强制撤回消息

```mermaid
sequenceDiagram
    autonumber
    participant Admin
    participant AS as AdminServer
    participant Bus as MsgBus
    participant Msg as MsgSvc
    participant DAO
    participant CM
    participant Conv as 会话成员

    Admin->>AS: POST /api/v1/messages/recall<br/>{ conv_id, server_seq, reason }
    AS->>AS: RequirePermission("message.recall")
    AS->>Bus: publish("message.admin_recall", payload)
    Bus-->>Msg: 订阅回调
    Msg->>DAO: Message.MarkRecalled(seq, by=admin)
    Msg->>CM: 广播 kRecallNotify(operator=Admin)
    CM-->>Conv: kRecallNotify
    AS->>DAO: AuditLog.Insert(...)
    AS-->>Admin: { code=0 }
```

---

## 31. Admin 群组解散 / 转让

```mermaid
sequenceDiagram
    autonumber
    participant Admin
    participant AS as AdminServer
    participant Group as GroupSvc (via Bus or in-proc)
    participant DAO
    participant CM
    participant Members

    alt 解散
        Admin->>AS: DELETE /api/v1/groups/:conv_id
        AS->>AS: RequirePermission("group.delete")
        AS->>Group: Dissolve(conv_id)
        Group->>DAO: Group.UpdateStatus(Dissolved)
        Group->>DAO: ConvMember.RemoveAll
        Group->>CM: 广播 GroupNotify(type=Dissolved)
        CM-->>Members: GroupNotify
    else 转让群主
        Admin->>AS: PATCH /api/v1/groups/:conv_id/owner<br/>{ new_owner_uid }
        AS->>AS: RequirePermission("group.transfer")
        AS->>DAO: ConvMember.Exists(conv_id, new_owner)
        AS->>DAO: Group.UpdateOwner(conv_id, new_owner)
        AS->>CM: 广播 GroupNotify(type=OwnerChanged)
    end
    AS->>DAO: AuditLog.Insert(...)
    AS-->>Admin: { code=0 }
```

---

## 32. MsgBus 跨服务事件传播

```mermaid
sequenceDiagram
    autonumber
    participant Pub as Publisher (UserSvc / AS / ...)
    participant Bus as MsgBus
    participant S1 as 订阅者 1 (CM)
    participant S2 as 订阅者 2 (StatsSvc)
    participant S3 as 订阅者 3 (AuditSvc)

    Pub->>Bus: publish<EventT>(payload)
    Bus->>Bus: 锁内拷贝订阅者列表
    Bus->>S1: callback(payload)（异步线程池）
    Bus->>S2: callback(payload)
    Bus->>S3: callback(payload)
    Note over Bus: 任一回调异常被捕获并记录<br/>不影响其他订阅者

    Note over S1: 取消订阅
    S1->>Bus: unsubscribe(sub_id)
    Bus->>Bus: 删除条目（受保护写）
```

