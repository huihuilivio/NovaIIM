# client/nova_sdk — NovaIIM C++ 客户端共享库
#
# 编译为动态库 nova_sdk (.dll/.so/.dylib)
# 供 Qt / iOS / Android / CLI 等上层调用

cmake_minimum_required(VERSION 3.28)

# ---- 源文件收集 ----
set(CLIENT_CORE_SOURCES
    core/client_config.cpp
    core/client_context.cpp
    core/reconnect_manager.cpp
    core/request_manager.cpp
)

set(CLIENT_SERVICE_SOURCES
    service/auth_service.cpp
    service/message_service.cpp
    service/sync_service.cpp
    service/profile_service.cpp
    service/friend_service.cpp
    service/conversation_service.cpp
    service/group_service.cpp
    service/file_service.cpp
)

set(CLIENT_VIEWMODEL_SOURCES
    viewmodel/ui_dispatcher.cpp
    viewmodel/nova_client.cpp
    viewmodel/app_vm.cpp
    viewmodel/login_vm.cpp
    viewmodel/chat_vm.cpp
    viewmodel/contact_vm.cpp
    viewmodel/conversation_vm.cpp
    viewmodel/group_vm.cpp
)

set(CLIENT_INFRA_SOURCES
    infra/logger.cpp
    infra/tcp_client.cpp
    infra/http_client.cpp
    infra/ws_client.cpp
    infra/timer.cpp
    infra/device_info.cpp
)

set(CLIENT_CACHE_SOURCES
    cache/cache_db.cpp
    cache/user_cache_dao.cpp
    cache/message_cache_dao.cpp
    cache/conversation_cache_dao.cpp
    cache/group_cache_dao.cpp
)

# ---- 共用 OBJECT 库 (避免源文件被编译两次 + 隔离 PDB) ----
add_library(nova_sdk_objs OBJECT
    ${CLIENT_CORE_SOURCES}
    ${CLIENT_SERVICE_SOURCES}
    ${CLIENT_VIEWMODEL_SOURCES}
    ${CLIENT_INFRA_SOURCES}
    ${CLIENT_CACHE_SOURCES}
)

target_include_directories(nova_sdk_objs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nova_sdk_objs PUBLIC
    nova_protocol
    spdlog::spdlog
    hv_static
    msgbus
    ormpp
)

nova_set_compiler_options(nova_sdk_objs)

# 动态库导出宏（仅 SHARED 需要）
target_compile_definitions(nova_sdk_objs PRIVATE NOVA_SDK_EXPORTS)

# ---- 动态库（默认对外发布） ----
add_library(nova_sdk SHARED $<TARGET_OBJECTS:nova_sdk_objs>)
target_link_libraries(nova_sdk PUBLIC nova_sdk_objs)
target_compile_definitions(nova_sdk PUBLIC NOVA_SDK_SHARED)
nova_set_output_dirs(nova_sdk)

# ---- 静态库（仅测试启用时构建，供单元测试链接以避免 DLL 导出符号问题） ----
if(NOVA_BUILD_TESTS)
    add_library(nova_sdk_static STATIC $<TARGET_OBJECTS:nova_sdk_objs>)
    target_link_libraries(nova_sdk_static PUBLIC nova_sdk_objs)
    nova_set_output_dirs(nova_sdk_static)
    # 不传播 NOVA_SDK_SHARED：测试代码取空宏定义，符号默认本地链接
endif()

# ---- 安装导出头文件 ----
install(FILES export.h DESTINATION include/nova/client)
install(DIRECTORY model/     DESTINATION include/nova/client/model     FILES_MATCHING PATTERN "*.h")
install(DIRECTORY service/   DESTINATION include/nova/client/service   FILES_MATCHING PATTERN "*.h")
install(DIRECTORY viewmodel/ DESTINATION include/nova/client/viewmodel FILES_MATCHING PATTERN "*.h")
install(DIRECTORY infra/     DESTINATION include/nova/client/infra     FILES_MATCHING PATTERN "*.h")
install(TARGETS nova_sdk DESTINATION lib)

# 注：单元测试由根 CMakeLists 在 testing.cmake 加载后统一添加 (client/nova_sdk/test)
