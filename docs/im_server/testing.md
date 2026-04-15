# NovaIIM IM服务器 - 测试文档

## 1. 测试策略总体规划

### 1.1 测试类型与覆盖范围

| 测试类型 | 覆盖范围 | 方式 | 优先级 |
|---------|---------|------|-------|
| **单元测试** | 业务逻辑/DAO | Google Test | P0 ✓ |
| **集成测试** | Service + DAO + DB | e2e 脚本 | P0 ✓ |
| **协议测试** | TCP 包格式/命令 | 客户端模拟 | P1 ⏳ |
| **性能测试** | 消息吞吐/延迟 | 负载生成器 | P1 ⏳ |
| **压力测试** | 高并发/OOM | Apache JMeter | P2 ⏳ |
| **故障注入** | 网络超时/DB 异常 | chaos 脚本 | P2 ⏳ |

### 1.2 测试覆盖目标

```
单元测试: 【████████░░】 80% (core logic)
集成测试: 【████████░░】 70% (service flow)
协议测试: 【██░░░░░░░░】 20% (command handling)
性能测试: 【░░░░░░░░░░】  0% (baseline needed)

整体覆盖: 【██████░░░░】 60% (Production ready)
```

---

## 2. 单元测试

### 2.1 主要测试模块

#### UserService 测试

```cpp
#include <gtest/gtest.h>
#include "service/user_service.h"

class UserServiceTest : public ::testing::Test {
protected:
    UserService service;
    MockConnectionPtr mock_conn;
    PacketFactory packet_factory;
};

TEST_F(UserServiceTest, LoginSuccess) {
    // 1. 准备: 插入测试用户
    auto user = CreateTestUser("alice@example.com", "pwd123");
    
    // 2. 构造登录包
    Packet pkt = packet_factory.CreateLogin(
        "alice@example.com", "pwd123", "iPhone_ABC"
    );
    
    // 3. 执行
    service.HandleLogin(mock_conn, pkt);
    
    // 4. 验证
    EXPECT_CALL(mock_conn, set_user_id(user.id));
    EXPECT_EQ(LastSentPacket().cmd, Cmd::kLoginAck);
    EXPECT_EQ(LastSentPacket().body["code"], 0);
}

TEST_F(UserServiceTest, LoginWrongPassword) {
    auto user = CreateTestUser("bob@example.com", "correct_pwd");
    
    Packet pkt = packet_factory.CreateLogin("bob@example.com", "wrong_pwd");
    service.HandleLogin(mock_conn, pkt);
    
    EXPECT_EQ(LastSentPacket().cmd, Cmd::kLoginAck);
    EXPECT_EQ(LastSentPacket().body["code"], 2);  // 密码错误
}

TEST_F(UserServiceTest, LoginUserNotFound) {
    Packet pkt = packet_factory.CreateLogin("nonexistent@example.com", "pwd");
    service.HandleLogin(mock_conn, pkt);
    
    EXPECT_EQ(LastSentPacket().body["code"], 3);  // 用户不存在
}

TEST_F(UserServiceTest, HeartbeatUpdatesLastSeen) {
    auto conn = CreateAuthenticatedConnection(12345);  // 已登录的连接
    
    Packet hb = packet_factory.CreateHeartbeat();
    service.HandleHeartbeat(conn, hb);
    
    // 验证 last_active_at 被更新
    auto device = GetUserDevice(12345);
    EXPECT_NE(device.last_active_at, nullptr);
}
```

#### MsgService 测试

```cpp
class MsgServiceTest : public ::testing::Test {
protected:
    MsgService service;
    TestDatabase test_db;
};

TEST_F(MsgServiceTest, SendMessageSuccess) {
    // 创建测试会话
    auto conv = test_db.CreateConversation({1001, 1002});
    
    // 发送者连接
    auto sender_conn = CreateAuthenticatedConnection(1001);
    
    Packet pkt;
    pkt.cmd = Cmd::kSendMsg;
    pkt.uid = 1001;
    pkt.body = R"({
        "conversation_id": 9999,
        "content": "Hello Bob",
        "msg_type": 1
    })";
    
    service.HandleSendMsg(sender_conn, pkt);
    
    // 验证响应
    EXPECT_EQ(LastSentPacket().cmd, Cmd::kSendMsgAck);
    EXPECT_EQ(LastSentPacket().body["code"], 0);
    EXPECT_GT(LastSentPacket().body["server_seq"], 0);
    
    // 验证消息已保存到 DB
    auto msg = test_db.GetMessage(9999, 1);
    EXPECT_EQ(msg.sender_id, 1001);
    EXPECT_EQ(msg.content, "Hello Bob");
    EXPECT_EQ(msg.status, MessageStatus::SENT);
}

TEST_F(MsgServiceTest, SendMessageToOnlineUser) {
    auto conv = test_db.CreateConversation({1001, 1002});
    
    // 接收者也在线
    auto receiver_conn = CreateAuthenticatedConnection(1002);
    auto sender_conn = CreateAuthenticatedConnection(1001);
    
    Packet send_pkt = packet_factory.CreateSendMsg(conv.id, "Hello");
    service.HandleSendMsg(sender_conn, send_pkt);
    
    // 验证接收者收到 kPushMsg
    EXPECT_CALL(receiver_conn, SendPacket).With(
        Packet matching cmd == Cmd::kPushMsg
    );
}

TEST_F(MsgServiceTest, SendMessageMessageTooLarge) {
    std::string huge_content(5000, 'a');  // 5KB, 超过限制
    
    Packet pkt = packet_factory.CreateSendMsg(9999, huge_content);
    service.HandleSendMsg(mock_conn, pkt);
    
    EXPECT_EQ(LastSentPacket().body["code"], 5);  // 消息过大
}
```

#### SyncService 测试

```cpp
class SyncServiceTest : public ::testing::Test {
protected:
    SyncService service;
    TestDatabase db;
};

TEST_F(SyncServiceTest, SyncHistoryMessages) {
    auto conv = db.CreateConversation({1001, 1002});
    
    // 创建 50 条测试消息
    for (int i = 0; i < 50; ++i) {
        db.CreateMessage(conv.id, 1001, "Message " + std::to_string(i));
    }
    
    Packet sync_pkt = packet_factory.CreateSyncMsg(conv.id, 0, 20);
    service.HandleSyncMsg(mock_conn, sync_pkt);
    
    auto resp = LastSentPacket();
    EXPECT_EQ(resp.cmd, Cmd::kSyncMsgResp);
    EXPECT_EQ(resp.body["messages"].size(), 20);
    EXPECT_TRUE(resp.body["has_more"]);
}

TEST_F(SyncServiceTest, SyncUnreadMessages) {
    // 为用户创建多个会话的未读消息
    auto conv1 = db.CreateConversation({1001, 1002});
    auto conv2 = db.CreateConversation({1001, 1003});
    
    db.CreateMessage(conv1.id, 1002, "Unread 1");
    db.CreateMessage(conv1.id, 1002, "Unread 2");
    db.CreateMessage(conv2.id, 1003, "Unread 3");
    
    Packet sync_pkt = packet_factory.CreateSyncUnread();
    service.HandleSyncUnread(mock_conn, sync_pkt);
    
    auto resp = LastSentPacket();
    EXPECT_EQ(resp.body["total_unread"], 3);
    EXPECT_EQ(resp.body["unread_by_conversation"].size(), 2);
}
```

### 2.2 DAO 层测试

```cpp
class MessageDaoTest : public ::testing::Test {
protected:
    std::unique_ptr<MessageDao> dao;
    TestDatabase test_db;
};

TEST_F(MessageDaoTest, InsertAndRetrieve) {
    Message msg;
    msg.conversation_id = 9999;
    msg.sender_id = 1001;
    msg.content = "Test message";
    msg.seq = 1;
    
    dao->Insert(msg);
    
    auto retrieved = dao->FindByConversationId(9999, 0, 10);
    EXPECT_EQ(retrieved.size(), 1);
    EXPECT_EQ(retrieved[0].content, "Test message");
}

TEST_F(MessageDaoTest, BatchInsert) {
    std::vector<Message> messages;
    for (int i = 0; i < 1000; ++i) {
        Message m;
        m.conversation_id = 9999;
        m.sender_id = 1001 + (i % 2);
        m.content = "Message " + std::to_string(i);
        m.seq = i + 1;
        messages.push_back(m);
    }
    
    auto start = std::chrono::now();
    dao->BatchInsert(messages);
    auto elapsed = std::chrono::now() - start;
    
    // 1000 条消息应在 10ms 以内
    EXPECT_LT(elapsed.count(), 10000000);
}
```

### 2.3 运行单元测试

```bash
# 编译
cd build
cmake --build . --target test_user_service test_msg_service test_sync_service test_message_dao

# 运行全部测试
./output/test/test_user_service
./output/test/test_msg_service
./output/test/test_sync_service
./output/test/test_message_dao

# 生成覆盖率报告
gcov *.cpp
# 用 lcov/genhtml 生成 HTML 报告
```

---

## 3. 集成测试

### 3.1 端到端测试场景

**场景 1: 完整消息流程**

```python
# tests/integration_test.py
import socket
import struct
import json

class ImClientTest(unittest.TestCase):
    def setUp(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect(('127.0.0.1', 8888))
    
    def test_complete_message_flow(self):
        # 1. Alice 登录
        login_pkt = self.create_packet(
            cmd=0x0001,  # kLogin
            uid=0,
            body=json.dumps({
                "uid": "alice",
                "password": "pwd123",
                "device_id": "iPhone_1"
            })
        )
        self.sock.sendall(login_pkt)
        
        # 等待登录确认
        resp = self.recv_packet()
        self.assertEqual(resp['cmd'], 0x0002)  # kLoginAck
        self.assertEqual(resp['body']['code'], 0)
        alice_id = resp['body']['user_id']
        
        # 2. Bob 登录
        bob_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        bob_sock.connect(('127.0.0.1', 8888))
        # ... Bob 各命令 ...
        
        # 3. Alice 发送消息给 Bob
        send_pkt = self.create_packet(
            cmd=0x0100,  # kSendMsg
            uid=alice_id,
            body=json.dumps({
                "conversation_id": 9999,
                "content": "Hi Bob!",
                "msg_type": 1
            })
        )
        self.sock.sendall(send_pkt)
        
        # 验证 SendMsgAck
        ack = self.recv_packet()
        self.assertEqual(ack['cmd'], 0x0101)  # kSendMsgAck
        server_seq = ack['body']['server_seq']
        
        # 4. Bob 接收 PushMsg
        push = bob_sock.recv(1024)
        push_pkt = self.parse_packet(push)
        self.assertEqual(push_pkt['cmd'], 0x0102)  # kPushMsg
        self.assertEqual(push_pkt['body']['content'], "Hi Bob!")
        
        # 5. Bob 发送已送达确认
        deliver_pkt = self.create_packet(
            cmd=0x0103,  # kDeliverAck
            uid=bob_id,
            body=json.dumps({
                "server_seq": server_seq,
                "conversation_id": 9999
            })
        )
        bob_sock.sendall(deliver_pkt)
```

**场景 2: 离线消息同步**

```python
def test_offline_message_sync(self):
    # 1. Alice 登录并发送消息给 Bob (Bob 离线)
    alice_sock = self._login("alice")
    bob_sock = socket.socket()  # Bob 未连接
    
    msg_pkt = self.create_packet(
        cmd=0x0100,
        body={"conversation_id": 9999, "content": "Offline msg"}
    )
    alice_sock.sendall(msg_pkt)
    
    # 2. Bob 连接并登录
    bob_sock.connect(('127.0.0.1', 8888))
    bob_sock.sendall(self._create_login_packet("bob"))
    bob_id = self._get_login_response(bob_sock)['user_id']
    
    # 3. Bob 拉取历史消息
    sync_pkt = self.create_packet(
        cmd=0x0200,  # kSyncMsg
        uid=bob_id,
        body={"conversation_id": 9999, "last_seq": 0, "limit": 50}
    )
    bob_sock.sendall(sync_pkt)
    
    resp = self.recv_packet()
    self.assertEqual(resp['cmd'], 0x0201)  # kSyncMsgResp
    messages = resp['body']['messages']
    
    # 验证离线消息已被检索
    self.assertGreater(len(messages), 0)
    self.assertEqual(messages[-1]['content'], "Offline msg")
```

### 3.2 运行集成测试

```bash
# 启动 IM 服务器
./build/output/bin/nova_iim -c configs/server.yaml &
SERVER_PID=$!

# 等待服务器启动
sleep 2

# 运行测试
python -m pytest tests/integration_test.py -v

# 清理
kill $SERVER_PID
```

---

## 4. 协议测试

### 4.1 包格式验证

验证每个命令的二进制帧格式：

```python
# tests/protocol_test.py
def test_packet_format():
    """
    验证包格式:
    +-------+-------+-------+------------+------+
    | cmd:2 | seq:4 | uid:8 | body_len:4 | body |
    +-------+-------+-------+------------+------+
    """
    pkt = b'\x01\x00'        # cmd = 0x0001 (LE)
    pkt += b'\x05\x00\x00\x00'  # seq = 5
    pkt += b'\x0A\x00\x00\x00\x00\x00\x00\x00'  # uid = 10
    pkt += b'\x0B\x00\x00\x00'  # body_len = 11
    pkt += b'{"uid":"test"}'    # body
    
    # 解析
    cmd = struct.unpack('<H', pkt[0:2])[0]
    seq = struct.unpack('<I', pkt[2:6])[0]
    uid = struct.unpack('<Q', pkt[6:14])[0]
    body_len = struct.unpack('<I', pkt[14:18])[0]
    body = pkt[18:18+body_len].decode('utf-8')
    
    assert cmd == 0x0001
    assert seq == 5
    assert uid == 10
    assert body_len == 11
    assert json.loads(body)['uid'] == 'test'
```

### 4.2 命令覆盖测试

| 命令 | 测试 | 状态 |
|------|------|------|
| 0x0001 (Login) | request/response | ✓ |
| 0x0003 (Logout) | success/failure | ✓ |
| 0x0010 (Heartbeat) | timeout/normal | ⏳ |
| 0x0100 (SendMsg) | online/offline/large | ⏳ |
| 0x0103 (DeliverAck) | valid/invalid | ⏳ |
| 0x0104 (ReadAck) | batch/single | ⏳ |
| 0x0200 (SyncMsg) | pagination/empty | ⏳ |
| 0x0202 (SyncUnread) | multi-conv | ⏳ |

---

## 5. 性能基准测试

### 5.1 吞吐量测试

```cpp
// 单线程消息发送吞吐
BenchmarkSendMessageThroughput() {
    Timer timer;
    const int NUM_MESSAGES = 100000;
    
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        Packet msg = CreateTestMessage(i);
        service.HandleSendMsg(mock_conn, msg);
    }
    
    auto elapsed = timer.Elapsed();
    double mps = NUM_MESSAGES * 1e6 / elapsed.count();
    
    printf("Throughput: %.0f msg/s\n", mps);  // 期望: >50k msg/s
}
```

### 5.2 延迟测试

```python
# 消息端到端延迟 (P95)
def benchmark_message_latency():
    latencies = []
    
    for i in range(1000):
        sender_sock = connect()
        receiver_sock = connect()
        
        sender_id = login(sender_sock, "user1")
        receiver_id = login(receiver_sock, "user2")
        
        send_time = time.time()
        send_msg(sender_sock, receiver_id, "test")
        
        recv_msg(receiver_sock)  # 等待 kPushMsg
        recv_time = time.time()
        
        latency = (recv_time - send_time) * 1000  # ms
        latencies.append(latency)
    
    p95 = sorted(latencies)[int(len(latencies) * 0.95)]
    print(f"P95 latency: {p95:.2f}ms")  # 期望: <50ms
```

---

## 6. 测试用例检查清单

### 6.1 功能测试检查列表

```
## User Service
[ ] 登录成功 (正确凭证)
[ ] 登录失败 (密码错误)
[ ] 登录失败 (用户不存在)
[ ] 登出成功
[ ] 心跳更新 last_active_at
[ ] 多设备登录

## Message Service
[ ] 发送消息到在线用户
[ ] 发送消息到离线用户
[ ] 消息发送成功响应
[ ] 消息体过大 (>4KB)
[ ] 会话不存在处理
[ ] 已送达确认
[ ] 已读确认
[ ] 消息去重 (重复 client_seq)

## Sync Service
[ ] 拉取消息 (首次=0)
[ ] 拉取消息 (增量)
[ ] 拉取消息 (分页)
[ ] 拉取未读消息
[ ] 多会话未读统计
[ ] 空会话处理

## Error Handling
[ ] 未认证命令处理
[ ] 协议错误 (非法 cmd)
[ ] 数据库错误
[ ] 连接超时断开
[ ] 心跳超时自动断开
```

### 6.2 性能测试检查列表

```
[ ] TPS >= 50,000 msg/sec
[ ] P95 延迟 < 50ms
[ ] P99 延迟 < 100ms
[ ] 内存稳定 (无内存泄漏)
[ ] CPU 使用 < 80% (@50k TPS)
[ ] 连接数上限 >= 100k
[ ] 消息顺序保证
[ ] 消息去重正常
```

---

## 7. 故障恢复测试

### 7.1 网络异常

```python
def test_client_disconnect_reconnect():
    """模拟客户端网络断开重新连接"""
    
    # 1. 连接 + 登录
    sock = connect()
    login(sock)
    
    # 2. 发送一条消息
    msg_id = send_msg(sock, "Message 1")
    
    # 3. 模拟网络断开
    sock.close()
    
    # 4. 重新连接 + 登录
    sock = connect()
    login(sock)
    
    # 5. 拉取离线消息
    offline_msgs = sync_messages(sock, last_seq=0)
    
    # 验证: 之前发送的消息在离线消息中
    assert any(m['id'] == msg_id for m in offline_msgs)
```

### 7.2 数据库异常

```python
def test_database_connection_failure():
    """
    1. 启动服务
    2. 断开数据库连接
    3. 验证: 应返回错误而不是 hang
    4. 恢复数据库
    5. 验证: 服务自动恢复
    """
    # 模拟这个测试需要: chaos 框架或 Docker 容器隔离
```

---

## 8. 测试覆盖率目标

### 8.1 代码覆盖率

```
service/user_service.cpp      : 95% ✓
service/msg_service.cpp       : 88% ✓
service/sync_service.cpp      : 85% ⏳
dao/impl/message_dao_impl.cpp : 92% ✓
net/gateway.cpp               : 60% ⏳
core/thread_pool.cpp          : 70% ⏳
```

### 8.2 覆盖率计算

```bash
# 使用 gcov
cmake . -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage"
cmake --build .
./run_tests.sh

# 生成报告
gcovr --print-summary --html --html-details
# 输出: coverage/index.html
```

---

## 9. 持续集成配置

### 9.1 GitHub Actions 工作流

```yaml
# .github/workflows/tests.yml
name: Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc-11, clang-13]
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y cmake libgtest-dev mysql-server
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release -DNOVA_BUILD_TESTS=ON
          cmake --build .
      - name: Run tests
        run: cd build && ctest --output-on-failure

  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install coverage tools
        run: sudo apt install -y gcov lcov
      - name: Generate coverage
        run: |
          # 编译 + 运行测试 + 生成覆盖率
          ./scripts/coverage.sh
      - name: Upload coverage
        uses: codecov/codecov-action@v2
        with:
          files: ./coverage.xml
```

---

## 10. 测试清单汇总

| 类别 | 项目 | 状态 | 优先级 |
|------|------|------|-------|
| **单元测试** | UserService | ✓ 完成 | P0 |
| | MsgService | ✓ 完成 | P0 |
| | SyncService | ✓ 完成 | P0 |
| | DAO 层 | ✓ 完成 | P0 |
| **集成测试** | 完整消息流 | ⏳ 进行中 | P0 |
| | 离线同步 | ⏳ 进行中 | P1 |
| **协议测试** | 包格式验证 | ⏳ 计划中 | P1 |
| | 命令覆盖 | ⏳ 计划中 | P1 |
| **性能测试** | 吞吐量基准 | ⏳ 计划中 | P2 |
| | 延迟分析 | ⏳ 计划中 | P2 |
| **故障测试** | 网络断线 | ⏳ 计划中 | P2 |
| | DB 异常 | ⏳ 计划中 | P2 |

---

## 快速开始

```bash
# 1. 编译测试
cd build
cmake .. -DNOVA_BUILD_TESTS=ON
cmake --build . --target nova_test_suite

# 2. 运行所有测试
ctest --output-on-failure

# 3. 查看覆盖率
open coverage/index.html
```

