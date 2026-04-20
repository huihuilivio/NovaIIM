# client/test — nova_sdk 单元测试

add_executable(test_nova_sdk
    test_request_manager.cpp
    test_connection_state.cpp
)

target_link_libraries(test_nova_sdk PRIVATE
    nova_sdk
    GTest::gtest_main
)

nova_set_compiler_options(test_nova_sdk)
nova_set_output_dirs(test_nova_sdk TEST)

# 将 nova_sdk.dll 复制到测试输出目录
add_custom_command(TARGET test_nova_sdk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:nova_sdk>
        $<TARGET_FILE_DIR:test_nova_sdk>
)

include(GoogleTest)
gtest_discover_tests(test_nova_sdk)
