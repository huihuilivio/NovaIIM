cmake_minimum_required(VERSION 3.28)
project(NovaIIM
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ---- CMake 模块 ----
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(compiler)
include(options)
include(utils)
include(dependencies)

# ---- 全局设置 ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 第三方动态库（.dll/.so）输出到 output/bin，确保可执行文件能找到
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NOVA_OUTPUT_DIR}/bin)

# ---- 第三方依赖 (FetchContent) ----
nova_fetch_all_dependencies()

# ---- 子项目 ----
add_subdirectory(protocol)

if(NOVA_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(NOVA_BUILD_CLIENT)
    add_subdirectory(client/cpp)
endif()

# ---- 测试 ----
if(NOVA_BUILD_TESTS)
    enable_testing()
    include(testing)
    add_subdirectory(server/test)
endif()

# ---- 构建信息 ----
nova_print_config()
