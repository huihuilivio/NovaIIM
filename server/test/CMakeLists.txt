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
