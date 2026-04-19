# client/test — nova_client 单元测试

add_executable(test_nova_client
    test_event_bus.cpp
    test_request_manager.cpp
    test_connection_state.cpp
)

target_link_libraries(test_nova_client PRIVATE
    nova_client
    GTest::gtest_main
)

nova_set_compiler_options(test_nova_client)
nova_set_output_dirs(test_nova_client TEST)

# 将 nova_client.dll 复制到测试输出目录
add_custom_command(TARGET test_nova_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:nova_client>
        $<TARGET_FILE_DIR:test_nova_client>
)

include(GoogleTest)
gtest_discover_tests(test_nova_client)
