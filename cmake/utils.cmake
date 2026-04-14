# utils.cmake - 工具函数

# 统一设置项目输出目录（仅对本项目 target 使用，不影响第三方库）
#
#   <build>/output/bin/         可执行文件
#   <build>/output/lib/         静态库 / 动态库
#   <build>/output/test/        测试可执行文件
#
# 用法:
#   nova_set_output_dirs(im_server)           → output/bin/
#   nova_set_output_dirs(im_server_lib)       → output/lib/
#   nova_set_output_dirs(test_router TEST)    → output/test/
#
set(NOVA_OUTPUT_DIR "${CMAKE_BINARY_DIR}/output" CACHE PATH "Project output root")

function(nova_set_output_dirs target)
    set(_is_test FALSE)
    if("TEST" IN_LIST ARGN)
        set(_is_test TRUE)
    endif()

    get_target_property(_type ${target} TYPE)

    if(_is_test)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${NOVA_OUTPUT_DIR}/test"
        )
    elseif(_type STREQUAL "EXECUTABLE")
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${NOVA_OUTPUT_DIR}/bin"
        )
    else()
        set_target_properties(${target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${NOVA_OUTPUT_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${NOVA_OUTPUT_DIR}/lib"
        )
    endif()
endfunction()

# 收集指定目录下所有源文件
function(nova_collect_sources dir out_var)
    file(GLOB_RECURSE _sources
        ${dir}/*.cpp
        ${dir}/*.cc
        ${dir}/*.h
        ${dir}/*.hpp
    )
    set(${out_var} ${_sources} PARENT_SCOPE)
endfunction()

# 打印构建信息
function(nova_print_config)
    message(STATUS "=== NovaIIM Build Configuration ===")
    message(STATUS "  Version:      ${PROJECT_VERSION}")
    message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
    message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "  Server:       ${NOVA_BUILD_SERVER}")
    message(STATUS "  Client:       ${NOVA_BUILD_CLIENT}")
    message(STATUS "  Tests:        ${NOVA_BUILD_TESTS}")
    message(STATUS "===================================")
endfunction()
