# IM 服务端架构设计（C++版本）

## 1. 设计目标

* 单进程高性能
* 无锁/低锁设计
* 模块解耦
* 可扩展到分布式

---

## 2. 技术选型

```text
网络：libhv / epoll
序列化：protobuf
日志：spdlog
DB：ormpp
线程模型：Reactor + 线程池
```

---

## 3. 整体架构

```text
                ┌──────────────┐
                │   Gateway    │
                └──────┬───────┘
                       │
                ┌──────▼──────┐
                │   Router    │
                └──────┬──────┘
                       │
        ┌──────────────┼──────────────┐
        │              │              │
   ┌────▼────┐   ┌────▼────┐   ┌────▼────┐
   │ UserSvc │   │ MsgSvc  │   │ SyncSvc │
   └────┬────┘   └────┬────┘   └────┬────┘
        │              │              │
        └──────┬───────┴──────┬──────┘
               │              │
        ┌──────▼──────┐ ┌─────▼─────┐
        │   DB Layer  │ │ ConnMgr   │
        └─────────────┘ └───────────┘
```

---

## 4. 核心模块设计

---

### 4.1 Gateway

职责：

* 管理连接
* WebSocket/TCP收发
* 心跳检测

```cpp
class Connection {
public:
    int64_t user_id;
    void Send(const Packet&);
};
```

---

### 4.2 ConnManager（多端）

```cpp
unordered_map<int64_t, vector<Connection*>>
```

---

### 4.3 Router

```cpp
class Router {
public:
    void Dispatch(Connection*, Packet&);
};
```

---

### 4.4 MsgService（核心）

职责：

* seq生成
* 写DB
* 推送消息
* ACK处理

---

### 4.5 SyncService

职责：

* 离线拉取
* 多端同步
* 未读管理

---

## 5. 消息流程（核心）

---

### 发送消息

```text
Client A
  ↓
Gateway
  ↓
MsgSvc
  ↓
1. 生成 seq
2. 写 DB
3. 返回 SEND_ACK
4. 推送给 B 所有端
5. 推送给 A 其他端
```

---

### ACK处理

```text
DELIVER_ACK → 标记已送达
READ_ACK → 更新 read_cursor
```

---

## 6. seq顺序保证

---

### 方案

```cpp
int64_t seq = repo.GetMaxSeq(conv_id) + 1;
```

---

### 优化

* 内存缓存
* CAS自增

---

## 7. 线程模型

---

### 推荐

```text
IO线程（libhv）
   ↓
消息队列（无锁队列）
   ↓
Worker线程池
```

---

## 8. 无锁队列（你之前研究的）

推荐：

* Vyukov MPMC

用途：

* 消息分发
* 异步处理

---

## 9. E2E设计

---

### 服务端

* 不解密
* 不存明文
* 不参与密钥交换

---

### 只负责

```text
转发 ciphertext
```

---

## 10. 多端同步

---

```cpp
for (auto conn : conn_mgr.Get(user_id)) {
    conn->Send(msg);
}
```

---

## 11. 扩展路径

---

### Phase 1（当前）

* 单进程

---

### Phase 2

* Redis（在线状态）
* seq优化

---

### Phase 3

* Gateway拆分
* MsgSvc拆分

---

### Phase 4

* Kafka
* 分库分表

---

## 12. 项目结构

```text
im-server/
├── net/
├── router/
├── service/
├── db/
├── model/
├── proto/
├── util/
```

---

## 13. 总结

该架构具备：

* ✔ 高性能（C++）
* ✔ 可扩展
* ✔ 支持百万连接演进
* ✔ 与Go版本架构一致

---

## 一句话定位

👉 一个可以直接演进成“企业级IM系统”的C++后端架构

---
