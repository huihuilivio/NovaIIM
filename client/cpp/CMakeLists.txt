# client/cpp — NovaIIM C++ 客户端共享库
#
# 编译为动态库 nova_client (.dll/.so/.dylib)
# 供 Qt / iOS / Android / CLI 等上层调用

cmake_minimum_required(VERSION 3.28)

# ---- 源文件收集 ----
set(CLIENT_CORE_SOURCES
    core/logger.cpp
    core/client_config.cpp
    core/event_bus.cpp
    core/ui_dispatcher.cpp
    core/client_context.cpp
)

set(CLIENT_NET_SOURCES
    net/tcp_client.cpp
    net/reconnect_manager.cpp
    net/request_manager.cpp
)

# ---- 动态库 ----
add_library(nova_client SHARED
    ${CLIENT_CORE_SOURCES}
    ${CLIENT_NET_SOURCES}
)

target_include_directories(nova_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}          # client/cpp/ → #include <core/xxx.h>
)

target_link_libraries(nova_client PUBLIC
    nova_protocol
    spdlog::spdlog
    hv_static
)

# 编译器选项（含 NOMINMAX / WIN32_LEAN_AND_MEAN）
nova_set_compiler_options(nova_client)

# 导出宏
target_compile_definitions(nova_client
    PRIVATE NOVA_CLIENT_EXPORTS
    PUBLIC  NOVA_CLIENT_SHARED
)

nova_set_output_dirs(nova_client)

# ---- 安装导出头文件 ----
install(DIRECTORY core/ DESTINATION include/nova/client/core FILES_MATCHING PATTERN "*.h")
install(DIRECTORY net/  DESTINATION include/nova/client/net  FILES_MATCHING PATTERN "*.h")
install(TARGETS nova_client DESTINATION lib)
