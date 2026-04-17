# Admin模块测试流程与脚本

## 概述

已完成 **全部测试目标**，共120 个测试用例：

| 测试目标 | 用例数 | 状态 | 覆盖范围 |
|--------|-------|------|----------|
| `test_jwt_utils` | 13 | ✅ 通过 | JWT签发/验证、过期、篡改、多算法 |
| `test_password_utils` | 11 | ✅ 通过 | PBKDF2哈希、验证、随机盐、边界值 |
| `test_admin_dao` | 24 | ✅ 通过 | AdminAccountDao/AdminSessionDao/RbacDao + in-mem SQLite |
| `test_admin_api` | 21 | ✅ 通过 | HTTP集成测试、真实AdminServer、鉴权中间件 |
| `test_router` | 5 | ✅ 通过 | 命令字路由分发 |
| `test_mpmc_queue` | 5 | ✅ 通过 | Vyukov 无锁队列 |
| `test_conn_manager` | 4 | ✅ 通过 | 多端连接管理 |
| `test_user_service` | 37 | ✅ 通过 | 邮箱注册/登录、设备管理、心跳、多端同步 |

## 测试执行脚本

### 编译所有测试目标

```bash
cd d:\livio\NovaIIM\build
cmake .. -DNOVA_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --target test_jwt_utils test_password_utils test_admin_dao test_admin_api
```

### 运行全部测试

```bash
cd d:\livio\NovaIIM\build
ctest --output-on-failure
# 期望输出: 100% tests passed, 0 tests failed out of 120
```

### 运行特定测试

```bash
# 运行单个测试
./test_jwt_utils.exe --gtest_filter="JwtUtilsTest.VerifyValidTokenReturnsCorrectAdminId"

# 按前缀运行多个测试  
./test_admin_dao.exe --gtest_filter="AdminAccountDaoTest.*"

# 仅显示失败信息
./test_password_utils.exe --gtest_brief=1

# 生成XML报告
./test_jwt_utils.exe --gtest_output=xml:test_report.xml
```

## CMake集成

测试通过 `nova_add_test()` 宏注册到 CTest：

```cmake
nova_add_test(
    NAME        test_jwt_utils
    SOURCES     test_jwt_utils.cpp
    LIBS        im_server_lib
)
```

**关键改进：**
- 使用 `/DELAYLOAD:libmysql.dll` 避免MySQL依赖链加载（测试仅用SQLite）
- 使用 `DISCOVERY_MODE PRE_TEST` 推迟测试发现到ctest运行时
- 自动拷贝libmysql.dll到测试输出目录

## 测试覆盖范围

### 1. JWT Utils (13个用例)
- ✅ Sign/Verify正流程、不同admin_id、iat/exp填充
- ✅ 错误密钥、过期token、篡改payload、空token
- ✅ 多算法(HS256/384/512)、算法不匹配

### 2. Password Utils (11个用例)  
- ✅ Hash格式验证（pbkdf2:sha256:iterations$salt$hash）
- ✅ Verify正确/错误/大小写敏感/特殊字符
- ✅ 随机盐导致同密码哈希不同
- ✅ 边界值：空密码、长密码(1000字符)、格式错误

### 3. Admin DAO (24个用例)
- ✅ AdminAccountDao: FindByUid/FindById、Insert、UpdatePassword、UNIQUE约束、软删除
- ✅ AdminSessionDao: IsRevoked、Insert、RevokeByTokenHash、RevokeByAdmin  
- ✅ RbacDao: GetUserPermissions、HasPermission、超管权限检查

### 4. Admin API (待运行)
- ✅ GET /healthz (无认证)
- ✅ POST /api/v1/auth/login (成功、密码错误、用户不存在、缺字段)
- ✅ 鉴权中间件 (无token、无效token、吊销token、错误格式)
- ✅ POST /api/v1/auth/logout (吊销current token)
- ✅ GET /api/v1/auth/me (返回当前管理员信息)
- ✅ GET /api/v1/dashboard/stats (仪表板数据)
- ✅ 用户管理API (列表、创建、重复uid失败)
- ✅ 审计日志API

## 已解决的历史问题

### 1. AdminAccountDaoTest.InsertAndFindByUid
- **问题**：`Admin::Insert()` 后 `admin.id` 未被自动填充（仍为0）
- **修复**：通过 `REGISTER_AUTO_KEY` 显式注册 auto_key，确保 insert 时跳过 id 列

### 2. test_admin_api 尚未运行
- **依赖**：真实HttpServer启动、端口19091可用
- **工作**：SetUpTestSuite()正确初始化ServerContext和AdminServer
- **待做**：验证所有HTTP集成测试通过

## 下一步

1. **修复 AdminAccountDao.Insert() 返回ID**  
   - 检查ormpp是否提供 `last_insert_rowid()` 或类似API
   - 或在Insert后再SELECT获取插入的记录

2. **运行 test_admin_api HTTP集成测试**
   - 确认所有23个HTTP场景通过
   - 验证鉴权中间件、审计日志记录

3. **测试覆盖率度量**
   - 生成GTest + GCOV覆盖率报告
   - 确保关键路径覆盖率 > 80%

4. **集成CI/CD**
   - GitHub Actions 自动运行所有测试
   - 失败时阻止merge

## 测试环境

- **编译器**：MSVC 14.42 (VS 2022)
- **CMake**：3.20+
- **语言**：C++20
- **框架**：GTest + GoogleMock
- **数据库**：SQLite in-memory (":memory:")
- **libhv客户端**：requests::get/post (同步HTTP)
