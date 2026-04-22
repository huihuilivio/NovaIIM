# server/test - 服务端单元测试

# ---- ConnManager 测试 ----
nova_add_test(
    NAME    test_conn_manager
    SOURCES test_conn_manager.cpp
    LIBS    im_server_lib
)

# ---- Router 测试 ----
nova_add_test(
    NAME    test_router
    SOURCES test_router.cpp
    LIBS    im_server_lib
)

# ---- MPMC Queue 测试 ----
nova_add_test(
    NAME    test_mpmc_queue
    SOURCES test_mpmc_queue.cpp
    LIBS    im_server_lib
)

# ---- Admin 模块单元测试 ----

# JwtUtils: JWT 签发/验证、过期、篡改、多算法
nova_add_test(
    NAME    test_jwt_utils
    SOURCES test_jwt_utils.cpp
    LIBS    im_server_lib
)

# PasswordUtils: PBKDF2 哈希格式、Verify 正/错、随机盐
nova_add_test(
    NAME    test_password_utils
    SOURCES test_password_utils.cpp
    LIBS    im_server_lib
)

# Admin DAO 层: AdminAccountDao / AdminSessionDao / RbacDao (in-memory SQLite)
nova_add_test(
    NAME    test_admin_dao
    SOURCES test_admin_dao.cpp
    LIBS    im_server_lib
)

# Admin HTTP 集成测试: 真实 AdminServer + HTTP 客户端 (in-memory SQLite, 端口 19091)
nova_add_test(
    NAME    test_admin_api
    SOURCES test_admin_api.cpp
    LIBS    im_server_lib
)

# UserService 登录链路: Register, Login, Logout, Heartbeat, 设备持久化, 多端踢下线
nova_add_test(
    NAME    test_user_service
    SOURCES test_user_service.cpp
    LIBS    im_server_lib
)

# Application 单元测试 + 集成测试
nova_add_test(
    NAME    test_application
    SOURCES test_application.cpp
    LIBS    im_server_lib
)

# FriendService 好友链路: AddFriend, HandleRequest, DeleteFriend, Block/Unblock, GetFriendList, GetRequests
nova_add_test(
    NAME    test_friend_service
    SOURCES test_friend_service.cpp
    LIBS    im_server_lib
)

# MsgService 消息链路: SendMsg, RecallMsg, DeliverAck, ReadAck, 幂等去重
nova_add_test(
    NAME    test_msg_service
    SOURCES test_msg_service.cpp
    LIBS    im_server_lib
)

# ConvService 会话管理: GetConvList, DeleteConv, MuteConv, PinConv, 自动恢复
nova_add_test(
    NAME    test_conv_service
    SOURCES test_conv_service.cpp
    LIBS    im_server_lib
)

# GroupService 群组管理: CreateGroup, DismissGroup, JoinGroup, LeaveGroup, KickMember, UpdateGroup, SetRole
nova_add_test(
    NAME    test_group_service
    SOURCES test_group_service.cpp
    LIBS    im_server_lib
)

# FileService 文件元数据: Upload, UploadComplete, Download
nova_add_test(
    NAME    test_file_service
    SOURCES test_file_service.cpp
    LIBS    im_server_lib
)

# FileServer HTTP 集成测试: 真实 FileServer + HTTP 客户端 (临时目录, 端口 19092)
nova_add_test(
    NAME    test_file_server
    SOURCES test_file_server.cpp
    LIBS    im_server_lib
)

# WsGateway WebSocket 网关: 二进制收发, 无效包断开, JSON 编解码
nova_add_test(
    NAME    test_ws_gateway
    SOURCES test_ws_gateway.cpp
    LIBS    im_server_lib
)
