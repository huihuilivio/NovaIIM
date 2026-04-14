# server/cpp/test - 服务端单元测试

# 所有测试共享的 include 路径
set(SERVER_SRC_DIR ${CMAKE_SOURCE_DIR}/server/cpp)

# ---- ConnManager 测试 ----
nova_add_test(
    NAME    test_conn_manager
    SOURCES test_conn_manager.cpp
    LIBS    nova_protocol
)
target_include_directories(test_conn_manager PRIVATE ${SERVER_SRC_DIR})

# ---- Router 测试 ----
nova_add_test(
    NAME    test_router
    SOURCES test_router.cpp
    LIBS    nova_protocol
)
target_include_directories(test_router PRIVATE ${SERVER_SRC_DIR})

# ---- MPMC Queue 测试 ----
nova_add_test(
    NAME    test_mpmc_queue
    SOURCES test_mpmc_queue.cpp
    LIBS    ""
)
target_include_directories(test_mpmc_queue PRIVATE ${SERVER_SRC_DIR})
