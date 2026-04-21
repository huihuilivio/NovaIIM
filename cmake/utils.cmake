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

# 构建 Web 前端 (Vue + Vite) 并部署到 target 输出目录
#
# 自动执行 npm install（首次）+ npm run build，构建后复制 dist/ 到输出目录。
# NOVA_NPM 由 cmake/environment.cmake 提供。
#
# 用法:
#   nova_add_web_frontend(
#       NAME       admin_web        # 自定义 target 名（唯一）
#       SOURCE_DIR ${dir}/web       # 前端源码目录（含 package.json）
#       OUTPUT_DIR admin            # 部署到 <exe>/admin/
#       DEPENDS    im_server        # 依赖的可执行 target
#   )
#
function(nova_add_web_frontend)
    cmake_parse_arguments(WEB "" "NAME;SOURCE_DIR;OUTPUT_DIR;DEPENDS" "" ${ARGN})

    if(NOT WEB_NAME OR NOT WEB_SOURCE_DIR OR NOT WEB_OUTPUT_DIR OR NOT WEB_DEPENDS)
        message(FATAL_ERROR "nova_add_web_frontend: NAME, SOURCE_DIR, OUTPUT_DIR, DEPENDS are required")
    endif()

    set(_dist_dir "${WEB_SOURCE_DIR}/dist")

    # npm install（仅当 node_modules 不存在时）
    if(NOT EXISTS "${WEB_SOURCE_DIR}/node_modules")
        message(STATUS "Running npm install in ${WEB_SOURCE_DIR}...")
        execute_process(
            COMMAND ${NOVA_NPM} install
            WORKING_DIRECTORY "${WEB_SOURCE_DIR}"
            RESULT_VARIABLE _npm_result
        )
        if(NOT _npm_result EQUAL 0)
            message(FATAL_ERROR "npm install failed in ${WEB_SOURCE_DIR}")
        endif()
    endif()

    # npm run build — 每次构建前执行
    add_custom_target(${WEB_NAME} ALL
        COMMAND ${NOVA_NPM} run build
        WORKING_DIRECTORY "${WEB_SOURCE_DIR}"
        COMMENT "Building ${WEB_NAME} (npm run build)..."
    )
    add_dependencies(${WEB_DEPENDS} ${WEB_NAME})

    # 复制构建产物到输出目录
    add_custom_command(TARGET ${WEB_DEPENDS} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_dist_dir}"
            "$<TARGET_FILE_DIR:${WEB_DEPENDS}>/${WEB_OUTPUT_DIR}"
        COMMENT "Deploying ${WEB_NAME} to ${WEB_OUTPUT_DIR}/..."
    )
endfunction()
