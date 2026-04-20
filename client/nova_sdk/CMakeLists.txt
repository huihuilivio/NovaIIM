# client/nova_sdk — NovaIIM C++ 客户端共享库
#
# 编译为动态库 nova_sdk (.dll/.so/.dylib)
# 供 Qt / iOS / Android / CLI 等上层调用

cmake_minimum_required(VERSION 3.28)

# ---- 源文件收集 ----
set(CLIENT_CORE_SOURCES
    core/client_config.cpp
    core/ui_dispatcher.cpp
    core/client_context.cpp
    core/reconnect_manager.cpp
    core/request_manager.cpp
)

set(CLIENT_INFRA_SOURCES
    infra/logger.cpp
    infra/tcp_client.cpp
    infra/http_client.cpp
    infra/ws_client.cpp
    infra/timer.cpp
)

# ---- 动态库 ----
add_library(nova_sdk SHARED
    ${CLIENT_CORE_SOURCES}
    ${CLIENT_INFRA_SOURCES}
)

target_include_directories(nova_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}          # client/nova_sdk/ → #include <core/xxx.h>
)

target_link_libraries(nova_sdk PUBLIC
    nova_protocol
    spdlog::spdlog
    hv_static
    msgbus
)

# 编译器选项（含 NOMINMAX / WIN32_LEAN_AND_MEAN）
nova_set_compiler_options(nova_sdk)

# 导出宏
target_compile_definitions(nova_sdk
    PRIVATE NOVA_SDK_EXPORTS
    PUBLIC  NOVA_SDK_SHARED
)

nova_set_output_dirs(nova_sdk)

# ---- 安装导出头文件 ----
install(FILES export.h DESTINATION include/nova/client)
install(DIRECTORY core/  DESTINATION include/nova/client/core  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY infra/ DESTINATION include/nova/client/infra FILES_MATCHING PATTERN "*.h")
install(TARGETS nova_sdk DESTINATION lib)
