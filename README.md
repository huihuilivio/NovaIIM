# NovaIIM 🚀

> A High-Performance Next-Gen Instant Messaging System (C++ / CMake)

---

## 📌 项目简介

**NovaIIM（Next Generation IM）** 是一个基于 C++ 构建的高性能即时通讯系统，采用现代 CMake 工程体系，支持：

* 实时通信（WebSocket）
* 多端同步
* 消息可靠投递（ACK + Seq）
* 模块化架构（便于扩展到分布式）

本项目专注于**纯 CMake 构建体系**，不依赖 Makefile，提供统一、跨平台的工程管理方案。

---

## 🏗️ 系统架构

```text
                ┌──────────────┐
                │   Admin Web  │
                └──────┬───────┘
                       │ HTTP
                ┌──────▼───────┐
                │  NovaIIM Server │
                │  (C++ / libhv)│
                └──────┬───────┘
          WebSocket    │
     ┌──────────┬──────▼──────┬──────────┐
     │          │             │          │
┌────▼────┐ ┌───▼────┐ ┌──────▼────┐ ┌──▼─────┐
│ PC客户端 │ │ 移动端 │ │ Web客户端 │ │ 机器人 │
└─────────┘ └────────┘ └───────────┘ └────────┘
```

---

## 🧱 技术栈

### 后端（Server）

* **C++20**
* **libhv**（网络通信：WebSocket / TCP）
* **yalantinglibs**（高性能序列化）
* **ormpp**（ORM）
* **spdlog**（日志）

### 客户端（PC）

* **C++**
* **Qt / QML**
* **SQLite3**（本地缓存）

### 管理后台（Admin）

* 前端：Vue / React
* 后端：Go / Node.js

### 构建系统

* **CMake ≥ 3.20（唯一构建方式）**

---

## 📁 项目结构

```text
NovaIIM/
├── CMakeLists.txt          # 顶层构建入口
├── cmake/                  # CMake工具模块
│   ├── compiler.cmake
│   ├── options.cmake
│   └── utils.cmake
│
│
├── configs/
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   └── db_design.sql
│
├── protocol/               # 协议层（序列化）
│
├── server/
│   └── cpp/
│       ├── net/
│       ├── service/
│       ├── dao/
│       └── model/
│
├── client/
│   └── cpp/
│       ├── net/
│       ├── core/
│       └── ui/
│
├── third_party/
│   ├── spdlog/
│   ├── ormpp/
│   └── yalantinglibs/
│   └── libhv/
└── admin/
```

---

## ⚙️ 核心功能

* 用户登录（JWT）
* 单聊（1v1）
* 消息 ACK 确认
* 消息顺序控制（Seq）
* 多端同步
* 未读计数
* 心跳机制
* 离线消息

---

## 🔁 消息流程

```text
Client → Server
   ↓
协议解码（yalanting）
   ↓
路由分发（cmd）
   ↓
业务处理
   ↓
数据库存储
   ↓
推送目标用户
   ↓
ACK返回
```

---

## 🔐 消息可靠性机制

### Seq（序列号）

* 客户端递增
* 防重复、防乱序

### ACK

* 服务端确认
* 客户端清除缓存

### 离线消息

* 服务端持久化
* 登录后同步

---

## 🧩 协议设计（简要）

```cpp
struct Packet {
    uint16_t cmd;
    uint32_t seq;
    uint64_t uid;
    std::string body;
};
```

---

## 🗄️ 数据库设计

核心表：

* users
* friends
* conversations
* messages
* unread
* groups / group_members

详见：`docs/db_design.sql`

---

## 🚀 快速开始（CMake）

### 1️⃣ 克隆项目

```bash
git clone https://github.com/yourname/NovaIIM.git
cd NovaIIM
```

---

### 2️⃣ 构建项目

```bash
mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

---

### 3️⃣ 运行服务端

```bash
./bin/im_server
```

---

## 🧪 开发模式

```bash
# Debug 构建
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 重新编译
cmake --build . -j

# 清理（删除 build 目录）
rm -rf build
```

---

## 📈 未来规划

* [ ] 群聊增强（权限 / 管理）
* [ ] 多媒体消息
* [ ] 分布式架构（网关层）
* [ ] 消息队列（Kafka / Redis）
* [ ] 推送系统（APNs / FCM）
* [ ] E2E 加密（Signal 简化版）

---

## 🧠 设计理念

* 纯 CMake 工程（跨平台 / 可维护）
* 模块解耦（protocol / net / core）
* 高性能优先（C++ + 事件驱动）
* 协议可控（替代 protobuf）

---

## 🤝 贡献

欢迎提交 Issue / PR，共同完善 NovaIIM。

---

## 📄 License

MIT License

---

## ⭐ 项目愿景

打造一个：

> **完全可控、可扩展、可演进的高性能 IM 基础设施**

---
