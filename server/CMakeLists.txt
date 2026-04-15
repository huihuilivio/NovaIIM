# server - NovaIIM 服务端

nova_collect_sources(${CMAKE_CURRENT_SOURCE_DIR} SERVER_SOURCES)

# 排除测试文件，测试在 test/CMakeLists.txt 中单独构建
list(FILTER SERVER_SOURCES EXCLUDE REGEX ".*/test/.*")

if(SERVER_SOURCES)
    # 分离 main.cpp，其余构建为静态库供测试复用
    set(SERVER_LIB_SOURCES ${SERVER_SOURCES})
    list(FILTER SERVER_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

    add_library(im_server_lib STATIC ${SERVER_LIB_SOURCES})
    nova_set_compiler_options(im_server_lib)
    nova_set_output_dirs(im_server_lib)
    target_include_directories(im_server_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # SPDLOG_ACTIVE_LEVEL: 编译期日志过滤（TRACE=0, DEBUG=1, INFO=2, ...）
    target_compile_definitions(im_server_lib PUBLIC
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    )
    target_link_libraries(im_server_lib PUBLIC
        nova_protocol
        hv_static
        spdlog::spdlog
        ylt
        sqlite3
        l8w8jwt
    )

    add_executable(im_server main.cpp)
    nova_set_compiler_options(im_server)
    nova_set_output_dirs(im_server)
    target_link_libraries(im_server PRIVATE im_server_lib CLI11::CLI11)
else()
    message(STATUS "[NovaIIM] Server sources not yet added, skipping im_server target")
endif()
