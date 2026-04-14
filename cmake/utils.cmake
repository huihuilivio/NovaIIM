# utils.cmake - 工具函数

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
