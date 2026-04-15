# dependencies.cmake - 第三方依赖统一管理（FetchContent）
#
# 策略：
#   1. 优先 find_package 检测系统已安装的库
#   2. 未找到则 FetchContent 自动下载
#   3. 所有依赖版本锁定，保证可复现构建
#
# 用法：
#   include(dependencies)  → 自动拉取所有依赖
#   target_link_libraries(xxx PRIVATE spdlog::spdlog hv_static ...)

include(FetchContent)

# 全局设置：下载到统一目录，避免重复下载
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/_deps CACHE PATH "FetchContent download dir")
set(FETCHCONTENT_QUIET OFF)

# 镜像源切换：无法访问 GitHub 时设置 -DNOVA_USE_GITEE=ON
option(NOVA_USE_GITEE "Use Gitee mirrors instead of GitHub" OFF)

if(NOVA_USE_GITEE)
    set(NOVA_GIT_SPDLOG       "https://gitee.com/mirrors/spdlog.git")
    set(NOVA_GIT_LIBHV        "https://gitee.com/libhv/libhv.git")
    set(NOVA_GIT_YALANTINGLIBS "https://gitee.com/alibaba/yalantinglibs.git")
    set(NOVA_GIT_GTEST        "https://gitee.com/mirrors/googletest.git")
    set(NOVA_GIT_CLI11        "https://gitee.com/mirrors/CLI11.git")
    message(STATUS "[NovaIIM] Using Gitee mirrors")
else()
    set(NOVA_GIT_SPDLOG       "https://github.com/gabime/spdlog.git")
    set(NOVA_GIT_LIBHV        "https://github.com/ithewei/libhv.git")
    set(NOVA_GIT_YALANTINGLIBS "https://github.com/alibaba/yalantinglibs.git")
    set(NOVA_GIT_GTEST        "https://github.com/google/googletest.git")
    set(NOVA_GIT_CLI11        "https://github.com/CLIUtils/CLI11.git")
endif()

# ============================================================
# 版本锁定
# ============================================================
set(NOVA_SPDLOG_VERSION       "v1.15.0")
set(NOVA_LIBHV_VERSION        "v1.3.3")
set(NOVA_YALANTINGLIBS_VERSION "0.3.9")
set(NOVA_GTEST_VERSION        "v1.15.2")
set(NOVA_CLI11_VERSION        "v2.4.2")

# ============================================================
# spdlog - 高性能日志
# https://github.com/gabime/spdlog
# ============================================================
macro(nova_fetch_spdlog)
    find_package(spdlog QUIET)
    if(NOT spdlog_FOUND)
        message(STATUS "[NovaIIM] Fetching spdlog ${NOVA_SPDLOG_VERSION} ...")
        FetchContent_Declare(spdlog
            GIT_REPOSITORY ${NOVA_GIT_SPDLOG}
            GIT_TAG        ${NOVA_SPDLOG_VERSION}
            GIT_SHALLOW    TRUE
        )
        # header-only 模式
        set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(spdlog)
        # 第三方头文件标记为 SYSTEM，抑制警告
        get_target_property(_spdlog_inc spdlog INTERFACE_INCLUDE_DIRECTORIES)
        set_target_properties(spdlog PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_spdlog_inc}")
    else()
        message(STATUS "[NovaIIM] Found system spdlog")
    endif()
endmacro()

# ============================================================
# libhv - 高性能网络库 (WebSocket / TCP / HTTP)
# https://github.com/ithewei/libhv
# ============================================================
macro(nova_fetch_libhv)
    find_package(libhv QUIET)
    if(NOT libhv_FOUND)
        message(STATUS "[NovaIIM] Fetching libhv ${NOVA_LIBHV_VERSION} ...")
        FetchContent_Declare(libhv
            GIT_REPOSITORY ${NOVA_GIT_LIBHV}
            GIT_TAG        ${NOVA_LIBHV_VERSION}
            GIT_SHALLOW    TRUE
        )
        # libhv 构建选项
        set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(WITH_OPENSSL OFF CACHE BOOL "" FORCE)
        set(WITH_EVPP ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(libhv)
        # 第三方头文件标记为 SYSTEM，抑制警告
        get_target_property(_hv_inc hv_static INTERFACE_INCLUDE_DIRECTORIES)
        set_target_properties(hv_static PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_hv_inc}")
    else()
        message(STATUS "[NovaIIM] Found system libhv")
    endif()
endmacro()

# ============================================================
# yalantinglibs - 高性能序列化/RPC (struct_pack, header-only)
# https://github.com/alibaba/yalantinglibs
# NOTE: yalantinglibs 作为子目录时不创建 target，手动建立 INTERFACE 库
# ============================================================
macro(nova_fetch_yalantinglibs)
    find_package(yalantinglibs QUIET)
    if(NOT yalantinglibs_FOUND)
        message(STATUS "[NovaIIM] Fetching yalantinglibs ${NOVA_YALANTINGLIBS_VERSION} ...")
        FetchContent_Declare(yalantinglibs
            GIT_REPOSITORY ${NOVA_GIT_YALANTINGLIBS}
            GIT_TAG        ${NOVA_YALANTINGLIBS_VERSION}
            GIT_SHALLOW    TRUE
        )
        FetchContent_GetProperties(yalantinglibs)
        if(NOT yalantinglibs_POPULATED)
            FetchContent_Populate(yalantinglibs)
        endif()
        # 创建 INTERFACE 库
        if(NOT TARGET ylt)
            add_library(ylt INTERFACE)
            target_include_directories(ylt SYSTEM INTERFACE
                ${yalantinglibs_SOURCE_DIR}/include
                ${yalantinglibs_SOURCE_DIR}/include/ylt/thirdparty
                ${yalantinglibs_SOURCE_DIR}/include/ylt/standalone
            )
            find_package(Threads REQUIRED)
            target_link_libraries(ylt INTERFACE Threads::Threads)
        endif()
    else()
        message(STATUS "[NovaIIM] Found system yalantinglibs")
    endif()
endmacro()

# ============================================================
# SQLite3 - 轻量嵌入式数据库 (第一版使用，后续可切 MySQL)
# 源码编译，无外部依赖
# 如果自动下载失败，手动下载 sqlite-amalgamation 并放到
# third_party/sqlite3/ 目录 (sqlite3.c + sqlite3.h)
# ============================================================
macro(nova_fetch_sqlite3)
    # 优先检查本地 vendor 目录
    set(_SQLITE3_LOCAL_DIR "${CMAKE_SOURCE_DIR}/third_party/sqlite3")
    if(EXISTS "${_SQLITE3_LOCAL_DIR}/sqlite3.c")
        message(STATUS "[NovaIIM] Using local sqlite3 from third_party/sqlite3/")
        set(_SQLITE3_SRC_DIR ${_SQLITE3_LOCAL_DIR})
    else()
        message(STATUS "[NovaIIM] Fetching sqlite3 amalgamation ...")
        FetchContent_Declare(sqlite3
            URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
        )
        FetchContent_GetProperties(sqlite3)
        if(NOT sqlite3_POPULATED)
            FetchContent_Populate(sqlite3)
        endif()
        set(_SQLITE3_SRC_DIR ${sqlite3_SOURCE_DIR})
    endif()
    if(NOT TARGET sqlite3)
        add_library(sqlite3 STATIC ${_SQLITE3_SRC_DIR}/sqlite3.c)
        target_include_directories(sqlite3 SYSTEM PUBLIC ${_SQLITE3_SRC_DIR})
        target_compile_definitions(sqlite3 PRIVATE
            SQLITE_THREADSAFE=1
            SQLITE_ENABLE_FTS5
            SQLITE_ENABLE_JSON1
        )
        if(WIN32)
            target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endif()
endmacro()

# ============================================================
# MySQL 客户端库 (预编译下载)
# 仅在 NOVA_ENABLE_MYSQL=ON 时启用
# 策略：
#   1) 通过 Python 脚本检测系统已安装的 MySQL / MariaDB 客户端
#   2) 未找到则自动下载当前系统对应的 MySQL 预编译包，提取 include/ 和 lib/
#   可通过 -DNOVA_MYSQL_VERSION=X.Y.Z 指定下载版本
# ============================================================
set(NOVA_MYSQL_VERSION "8.0.41" CACHE STRING "MySQL version to download if not found")
set(NOVA_MYSQL_MAJOR   "8.0"    CACHE STRING "MySQL major version series")

macro(nova_fetch_mysql_client)
    if(NOT TARGET mysql_client)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(NOT Python3_FOUND)
            message(FATAL_ERROR "[NovaIIM] Python3 is required to detect/download MySQL client library")
        endif()

        set(_MYSQL_INSTALL_DIR "${CMAKE_BINARY_DIR}/_mysql_client")

        message(STATUS "[NovaIIM] Running fetch_mysql_client.py ...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/cmake/fetch_mysql_client.py
                    ${_MYSQL_INSTALL_DIR}
                    --version ${NOVA_MYSQL_VERSION}
                    --major   ${NOVA_MYSQL_MAJOR}
            OUTPUT_VARIABLE _MYSQL_JSON
            ERROR_VARIABLE  _MYSQL_LOG
            RESULT_VARIABLE _MYSQL_RC
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        # 打印脚本日志 (stderr)
        if(_MYSQL_LOG)
            message(STATUS ${_MYSQL_LOG})
        endif()
        if(NOT _MYSQL_RC EQUAL 0)
            message(FATAL_ERROR "[NovaIIM] fetch_mysql_client.py failed (rc=${_MYSQL_RC})")
        endif()

        # 解析 JSON 输出
        string(JSON _MYSQL_INC GET ${_MYSQL_JSON} "include_dir")
        string(JSON _MYSQL_LIBDIR GET ${_MYSQL_JSON} "lib_dir")

        message(STATUS "[NovaIIM] MySQL client include: ${_MYSQL_INC}")
        message(STATUS "[NovaIIM] MySQL client lib dir: ${_MYSQL_LIBDIR}")

        # 自动查找 lib 文件
        file(GLOB _MYSQL_LIBS
            "${_MYSQL_LIBDIR}/libmysql.lib"
            "${_MYSQL_LIBDIR}/mysqlclient.lib"
            "${_MYSQL_LIBDIR}/libmysqlclient.a"
            "${_MYSQL_LIBDIR}/libmysqlclient.so"
            "${_MYSQL_LIBDIR}/libmysqlclient.dylib"
        )
        if(NOT _MYSQL_LIBS)
            message(FATAL_ERROR "[NovaIIM] No MySQL client library found in ${_MYSQL_LIBDIR}")
        endif()
        list(GET _MYSQL_LIBS 0 _MYSQL_LIB)
        message(STATUS "[NovaIIM] MySQL client library: ${_MYSQL_LIB}")

        add_library(mysql_client INTERFACE)
        target_include_directories(mysql_client SYSTEM INTERFACE ${_MYSQL_INC})
        target_link_libraries(mysql_client INTERFACE ${_MYSQL_LIB})
        if(WIN32)
            target_link_libraries(mysql_client INTERFACE ws2_32 shlwapi)
            # 把 DLL 拷贝到输出目录
            file(GLOB _MYSQL_DLLS "${_MYSQL_LIBDIR}/*.dll")
            if(_MYSQL_DLLS)
                file(COPY ${_MYSQL_DLLS} DESTINATION ${NOVA_OUTPUT_DIR}/bin)
            endif()
        endif()
    endif()
endmacro()

# ============================================================
# ormpp - C++20 ORM (header-only, 内置 iguana 反射)
# https://github.com/qicosmos/ormpp
# SQLite3 默认启用；MySQL 通过 -DNOVA_ENABLE_MYSQL=ON 启用
# ============================================================
macro(nova_fetch_ormpp)
    message(STATUS "[NovaIIM] Fetching ormpp ...")
    FetchContent_Declare(ormpp
        GIT_REPOSITORY https://github.com/qicosmos/ormpp.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(ormpp)
    if(NOT ormpp_POPULATED)
        FetchContent_Populate(ormpp)
    endif()

    if(NOT TARGET ormpp)
        add_library(ormpp INTERFACE)
        target_compile_definitions(ormpp INTERFACE ORMPP_ENABLE_SQLITE3)
        target_include_directories(ormpp SYSTEM INTERFACE
            ${ormpp_SOURCE_DIR}
            ${ormpp_SOURCE_DIR}/ormpp
            ${ormpp_SOURCE_DIR}/iguana
            ${ormpp_SOURCE_DIR}/frozen/include
        )
        target_link_libraries(ormpp INTERFACE sqlite3)

        # MySQL 支持（可选，mysql_client 由 nova_fetch_mysql_client 提供）
        if(NOVA_ENABLE_MYSQL)
            target_compile_definitions(ormpp INTERFACE ORMPP_ENABLE_MYSQL)
            target_link_libraries(ormpp INTERFACE mysql_client)
        endif()
    endif()
endmacro()

# ============================================================
# GoogleTest - 单元测试框架
# https://github.com/google/googletest
# 仅在 NOVA_BUILD_TESTS=ON 时拉取
# ============================================================
macro(nova_fetch_gtest)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "[NovaIIM] Fetching GoogleTest ${NOVA_GTEST_VERSION} ...")
        FetchContent_Declare(googletest
            GIT_REPOSITORY ${NOVA_GIT_GTEST}
            GIT_TAG        ${NOVA_GTEST_VERSION}
            GIT_SHALLOW    TRUE
        )
        # 防止 gtest 覆盖父项目编译器选项 (MSVC)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    else()
        message(STATUS "[NovaIIM] Found system GTest")
    endif()
endmacro()

# ============================================================
# CLI11 - 命令行参数解析 (header-only)
# https://github.com/CLIUtils/CLI11
# ============================================================
macro(nova_fetch_cli11)
    find_package(CLI11 QUIET)
    if(NOT CLI11_FOUND)
        message(STATUS "[NovaIIM] Fetching CLI11 ${NOVA_CLI11_VERSION} ...")
        FetchContent_Declare(cli11
            GIT_REPOSITORY ${NOVA_GIT_CLI11}
            GIT_TAG        ${NOVA_CLI11_VERSION}
            GIT_SHALLOW    TRUE
        )
        set(CLI11_PRECOMPILED OFF CACHE BOOL "" FORCE)
        set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(cli11)
    else()
        message(STATUS "[NovaIIM] Found system CLI11")
    endif()
endmacro()

# ============================================================
# l8w8jwt - 轻量 JWT 库 (纯 C, 内置 MbedTLS, 无 OpenSSL 依赖)
# https://github.com/GlitchedPolygons/l8w8jwt
# ============================================================
set(NOVA_L8W8JWT_VERSION "2.5.0")

macro(nova_fetch_l8w8jwt)
    message(STATUS "[NovaIIM] Fetching l8w8jwt ${NOVA_L8W8JWT_VERSION} ...")
    FetchContent_Declare(l8w8jwt
        GIT_REPOSITORY https://github.com/GlitchedPolygons/l8w8jwt.git
        GIT_TAG        ${NOVA_L8W8JWT_VERSION}
    )
    # 禁用 l8w8jwt 自带的测试和示例
    set(L8W8JWT_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(L8W8JWT_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(L8W8JWT_ENABLE_EDDSA OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(l8w8jwt)
    # 第三方头文件标记为 SYSTEM，抑制警告
    target_include_directories(l8w8jwt SYSTEM INTERFACE
        ${l8w8jwt_SOURCE_DIR}/include
        ${l8w8jwt_SOURCE_DIR}/lib/mbedtls/include
    )
endmacro()

# ============================================================
# 一键拉取所有依赖
# ============================================================
macro(nova_fetch_all_dependencies)
    message(STATUS "")
    message(STATUS "=== NovaIIM: Fetching Dependencies ===")

    nova_fetch_spdlog()
    nova_fetch_libhv()
    nova_fetch_yalantinglibs()
    nova_fetch_sqlite3()
    if(NOVA_ENABLE_MYSQL)
        nova_fetch_mysql_client()
    endif()
    nova_fetch_ormpp()
    nova_fetch_cli11()
    nova_fetch_l8w8jwt()

    if(NOVA_BUILD_TESTS)
        nova_fetch_gtest()
    endif()

    message(STATUS "=== NovaIIM: All Dependencies Ready ===")
    message(STATUS "")
endmacro()
