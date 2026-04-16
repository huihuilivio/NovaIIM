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
