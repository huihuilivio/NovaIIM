# testing.cmake - 测试工具函数

include(CTest)

# 添加一个测试可执行文件并自动注册到 CTest
#
# 用法:
#   nova_add_test(
#       NAME        test_msg_service
#       SOURCES     test_msg_service.cpp
#       LIBS        nova_protocol spdlog::spdlog
#   )
function(nova_add_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES;LIBS" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "nova_add_test: NAME is required")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nova_add_test: SOURCES is required")
    endif()

    add_executable(${ARG_NAME} ${ARG_SOURCES})
    nova_set_compiler_options(${ARG_NAME})
    nova_set_output_dirs(${ARG_NAME} TEST)

    target_link_libraries(${ARG_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        ${ARG_LIBS}
    )

    # 拷贝 libmysql.dll 到测试输出目录（Windows 运行时依赖）
    if(WIN32 AND NOVA_ENABLE_MYSQL)
        add_custom_command(TARGET ${ARG_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_BINARY_DIR}/_mysql_client/lib/libmysql.dll"
                "$<TARGET_FILE_DIR:${ARG_NAME}>"
            COMMENT "Copying libmysql.dll to test output directory"
        )
        # 延迟加载 libmysql.dll：测试仅使用 SQLite，不调用 MySQL 函数，
        # 因此无需在进程启动时解析 MySQL 依赖链（含 libssl/libcrypto）
        target_link_options(${ARG_NAME} PRIVATE /DELAYLOAD:libmysql.dll)
        target_link_libraries(${ARG_NAME} PRIVATE delayimp)
    endif()

    # 自动发现并注册测试用例
    # 使用 PRE_TEST 模式推迟发现到 ctest 运行时，避免构建时依赖 DLL
    include(GoogleTest)
    gtest_discover_tests(${ARG_NAME}
        WORKING_DIRECTORY ${NOVA_OUTPUT_DIR}/test
        DISCOVERY_MODE    PRE_TEST
    )
endfunction()
