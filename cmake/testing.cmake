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

    target_link_libraries(${ARG_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        ${ARG_LIBS}
    )

    # 自动发现并注册测试用例
    include(GoogleTest)
    gtest_discover_tests(${ARG_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endfunction()
