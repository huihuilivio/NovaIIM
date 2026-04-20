# client/test — nova_sdk 测试
#
# 仅通过公共 API (NovaClient / ViewModel) 进行测试
# 不直接测试 core/ / service/ 等内部实现

# TODO: 添加测试文件后取消注释
# add_executable(test_nova_sdk
#     test_xxx.cpp
# )
#
# target_link_libraries(test_nova_sdk PRIVATE
#     nova_sdk
#     GTest::gtest_main
# )
#
# nova_set_compiler_options(test_nova_sdk)
# nova_set_output_dirs(test_nova_sdk TEST)
#
# add_custom_command(TARGET test_nova_sdk POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         $<TARGET_FILE:nova_sdk>
#         $<TARGET_FILE_DIR:test_nova_sdk>
# )
#
# include(GoogleTest)
# gtest_discover_tests(test_nova_sdk)
