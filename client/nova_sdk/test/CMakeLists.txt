# client/nova_sdk/test - 客户端 SDK 单元测试
#
# 链接静态库 nova_sdk_static 以避免 DLL 导出符号 (__declspec(dllexport)) 问题
# 默认发布版本仍使用动态库 nova_sdk

# RequestManager: seq 匹配/超时/取消/线程安全
nova_add_test(
    NAME    test_request_manager
    SOURCES test_request_manager.cpp
    LIBS    nova_sdk_static
)

# ClientState: 枚举值稳定 + 字符串映射
nova_add_test(
    NAME    test_client_state
    SOURCES test_client_state.cpp
    LIBS    nova_sdk_static
)

# ReconnectManager: 启停状态 + 调度回调
nova_add_test(
    NAME    test_reconnect_manager
    SOURCES test_reconnect_manager.cpp
    LIBS    nova_sdk_static
)
