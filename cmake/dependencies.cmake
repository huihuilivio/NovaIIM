# dependencies.cmake - 第三方依赖统一管理（FetchContent）
#
# 策略：
#   1. 优先 find_package 检测系统已安装的库
#   2. 未找到则 FetchContent 自动下载
#   3. 所有依赖版本锁定，保证可复现构建
#   4. 第三方头文件标记 SYSTEM（抑制警告），EXCLUDE_FROM_ALL（不安装）
#
# 用法：
#   include(dependencies)  → 自动拉取所有依赖
#   target_link_libraries(xxx PRIVATE spdlog::spdlog hv ...)

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# ============================================================
# 版本锁定
# ============================================================
set(NOVA_SPDLOG_VERSION       "v1.17.0")
set(NOVA_LIBHV_VERSION        "v1.3.4")
set(NOVA_YALANTINGLIBS_VERSION "0.6.0")
set(NOVA_GTEST_VERSION        "v1.17.0")
set(NOVA_CLI11_VERSION        "v2.6.2")
set(NOVA_ORMPP_VERSION        "0.2.1")
set(NOVA_L8W8JWT_VERSION      "2.5.0")

# 镜像源切换：无法访问 GitHub 时设置 -DNOVA_USE_GITEE=ON
option(NOVA_USE_GITEE "Use Gitee mirrors instead of GitHub" ON)
if(NOVA_USE_GITEE)
    set(NOVA_GIT_LIBHV          "https://gitee.com/libhv/libhv.git")
    set(NOVA_GIT_YALANTINGLIBS  "https://gitee.com/alibaba/yalantinglibs.git")
    set(NOVA_GIT_GTEST          "https://gitee.com/mirrors/googletest.git")
    set(NOVA_GIT_ORMPP          "https://gitee.com/qicosmos/ormpp.git")
    # Gitee ormpp 镜像 tag 滞后，使用 master 分支
    set(NOVA_ORMPP_VERSION "master")
    message(STATUS "[NovaIIM] Using Gitee mirrors (ormpp pinned to master)")
else()
    set(NOVA_GIT_LIBHV          "https://github.com/ithewei/libhv.git")
    set(NOVA_GIT_YALANTINGLIBS  "https://github.com/alibaba/yalantinglibs.git")
    set(NOVA_GIT_GTEST          "https://github.com/google/googletest.git")
    set(NOVA_GIT_ORMPP          "https://github.com/qicosmos/ormpp.git")
endif()


# ============================================================
# spdlog - 高性能日志
# https://github.com/gabime/spdlog
# 源码已 vendor 到 thirdparty/spdlog
# ============================================================
macro(nova_fetch_spdlog)
    find_package(spdlog QUIET)
    if(NOT spdlog_FOUND)
        set(_SPDLOG_DIR "${CMAKE_SOURCE_DIR}/thirdparty/spdlog")
        if(NOT EXISTS "${_SPDLOG_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "[NovaIIM] thirdparty/spdlog not found.")
        endif()
        message(STATUS "[NovaIIM] Using local spdlog from thirdparty/")
        # 使用 bundled fmt（不依赖外部 fmt）
        set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
        add_subdirectory(${_SPDLOG_DIR} ${CMAKE_BINARY_DIR}/_spdlog EXCLUDE_FROM_ALL SYSTEM)
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
            SYSTEM
            EXCLUDE_FROM_ALL
        )
        # libhv 构建选项（动态库）
        set(BUILD_SHARED ON CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(WITH_OPENSSL OFF CACHE BOOL "" FORCE)
        set(WITH_EVPP ON CACHE BOOL "" FORCE)
        set(ENABLE_HLOG_FILE OFF CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(libhv)

        # 设置 hv.dll 输出到 bin 目录，并拷贝到 test 目录供测试使用
        if(WIN32 AND TARGET hv)
            set_target_properties(hv PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${NOVA_OUTPUT_DIR}/bin"
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${NOVA_OUTPUT_DIR}/bin"
                RUNTIME_OUTPUT_DIRECTORY_RELEASE "${NOVA_OUTPUT_DIR}/bin"
            )
            add_custom_target(nova_copy_hv_dll ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:hv>
                    ${NOVA_OUTPUT_DIR}/test/$<TARGET_FILE_NAME:hv>
                COMMENT "[NovaIIM] Copying hv.dll to output/test"
            )
            add_dependencies(nova_copy_hv_dll hv)
        endif()
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
            SOURCE_SUBDIR  _none  # 仅下载，不 add_subdirectory（手动创建 INTERFACE 库）
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(yalantinglibs)
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
# 内置 thirdparty/sqlite3，无需单独拉取
# MySQL 通过 -DNOVA_ENABLE_MYSQL=ON 启用
# ============================================================
macro(nova_fetch_ormpp)
    message(STATUS "[NovaIIM] Fetching ormpp ${NOVA_ORMPP_VERSION} ...")
    FetchContent_Declare(ormpp
        GIT_REPOSITORY ${NOVA_GIT_ORMPP}
        GIT_TAG        ${NOVA_ORMPP_VERSION}
        SOURCE_SUBDIR  _none  # 仅下载，不 add_subdirectory（手动创建 INTERFACE 库）
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(ormpp)

    # 使用 ormpp 内置的 sqlite3 源码编译
    if(NOT TARGET sqlite3)
        set(_ORMPP_SQLITE3_DIR ${ormpp_SOURCE_DIR}/thirdparty/sqlite3)
        add_library(sqlite3 STATIC ${_ORMPP_SQLITE3_DIR}/sqlite3.c)
        target_include_directories(sqlite3 SYSTEM PUBLIC ${_ORMPP_SQLITE3_DIR})
        target_compile_definitions(sqlite3 PRIVATE
            SQLITE_THREADSAFE=1
            SQLITE_ENABLE_FTS5
            SQLITE_ENABLE_JSON1
        )
        if(WIN32)
            target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
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
            SYSTEM
            EXCLUDE_FROM_ALL
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
# 源码已 vendor 到 thirdparty/cli11
# ============================================================
macro(nova_fetch_cli11)
    find_package(CLI11 QUIET)
    if(NOT CLI11_FOUND)
        set(_CLI11_DIR "${CMAKE_SOURCE_DIR}/thirdparty/cli11")
        if(NOT EXISTS "${_CLI11_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "[NovaIIM] thirdparty/cli11 not found.")
        endif()
        message(STATUS "[NovaIIM] Using local CLI11 from thirdparty/")
        set(CLI11_PRECOMPILED OFF CACHE BOOL "" FORCE)
        set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(${_CLI11_DIR} ${CMAKE_BINARY_DIR}/_cli11 EXCLUDE_FROM_ALL SYSTEM)
    else()
        message(STATUS "[NovaIIM] Found system CLI11")
    endif()
endmacro()

# ============================================================
# l8w8jwt - 轻量 JWT 库 (纯 C, 内置 MbedTLS, 无 OpenSSL 依赖)
# https://github.com/GlitchedPolygons/l8w8jwt
# 源码已 vendor 到 thirdparty/l8w8jwt（含 mbedtls 等子模块）
# ============================================================

macro(nova_fetch_l8w8jwt)
    set(_L8W8JWT_DIR "${CMAKE_SOURCE_DIR}/thirdparty/l8w8jwt")
    if(NOT EXISTS "${_L8W8JWT_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "[NovaIIM] thirdparty/l8w8jwt not found. "
            "Run: git submodule update --init, or manually place l8w8jwt sources there.")
    endif()

    message(STATUS "[NovaIIM] Using local l8w8jwt from thirdparty/")

    # 禁用 l8w8jwt 自带的测试和示例
    set(L8W8JWT_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(L8W8JWT_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(L8W8JWT_ENABLE_EDDSA OFF CACHE BOOL "" FORCE)
    # MbedTLS 选项：保存/恢复 ENABLE_TESTING 防止污染父项目
    set(_NOVA_SAVE_ENABLE_TESTING ${ENABLE_TESTING})
    set(_NOVA_SAVE_ENABLE_PROGRAMS ${ENABLE_PROGRAMS})
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)

    add_subdirectory(${_L8W8JWT_DIR} ${CMAKE_BINARY_DIR}/_l8w8jwt EXCLUDE_FROM_ALL SYSTEM)

    set(ENABLE_TESTING ${_NOVA_SAVE_ENABLE_TESTING} CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS ${_NOVA_SAVE_ENABLE_PROGRAMS} CACHE BOOL "" FORCE)
    # 补充 l8w8jwt 和 MbedTLS 头文件路径（上游 CMake 未正确导出）
    target_include_directories(l8w8jwt SYSTEM INTERFACE
        ${_L8W8JWT_DIR}/include
        ${_L8W8JWT_DIR}/lib/mbedtls/include
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
