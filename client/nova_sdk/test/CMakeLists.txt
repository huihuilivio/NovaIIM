# client/nova_sdk/test - 客户端 SDK 单元测试

# RequestManager: seq 匹配/超时/取消/线程安全
nova_add_test(
    NAME    test_request_manager
    SOURCES test_request_manager.cpp
    LIBS    nova_sdk
)

# ClientState: 枚举值稳定 + 字符串映射
nova_add_test(
    NAME    test_client_state
    SOURCES test_client_state.cpp
    LIBS    nova_sdk
)

# ReconnectManager: 启停状态 + 调度回调
nova_add_test(
    NAME    test_reconnect_manager
    SOURCES test_reconnect_manager.cpp
    LIBS    nova_sdk
)

# Windows: 复制 nova_sdk.dll 到测试输出目录（运行时依赖解析）
if(WIN32)
    foreach(_t test_request_manager test_client_state test_reconnect_manager)
        add_custom_command(TARGET ${_t} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nova_sdk>"
                "$<TARGET_FILE_DIR:${_t}>"
            COMMENT "Copying nova_sdk.dll to ${_t} output directory"
        )
    endforeach()
endif()
