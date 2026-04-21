# client/desktop/win — NovaIIM WebView2 桌面客户端 (Windows)
#
# 依赖: WebView2 SDK + nova_sdk 共享库

# ---- 下载 WebView2 SDK (NuGet) ----
set(WEBVIEW2_VERSION "1.0.2903.40")
set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/_webview2")
set(WEBVIEW2_NUPKG "${WEBVIEW2_DIR}/webview2.zip")

if(NOT EXISTS "${WEBVIEW2_DIR}/build/native/include/WebView2.h")
    message(STATUS "Downloading WebView2 SDK ${WEBVIEW2_VERSION}...")
    file(DOWNLOAD
        "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
        "${WEBVIEW2_NUPKG}"
        SHOW_PROGRESS
        STATUS download_status
    )
    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        message(WARNING "Failed to download WebView2 SDK — skipping desktop build")
        return()
    endif()
    file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_NUPKG}" DESTINATION "${WEBVIEW2_DIR}")
endif()

# ---- WebView2 imported target ----
add_library(webview2_sdk STATIC IMPORTED)
set_target_properties(webview2_sdk PROPERTIES
    IMPORTED_LOCATION "${WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${WEBVIEW2_DIR}/build/native/include"
)

message(STATUS "WebView2 SDK ${WEBVIEW2_VERSION} — building desktop client")

# ---- 可执行文件 ----
add_executable(nova_desktop WIN32
    main.cpp
    webview2_app.h
    webview2_app.cpp
    win32_ui_dispatcher.h
    win32_ui_dispatcher.cpp
    js_bridge.h
    js_bridge.cpp
    app.rc
)

target_link_libraries(nova_desktop PRIVATE
    nova_sdk
    webview2_sdk
)

nova_set_compiler_options(nova_desktop)
nova_set_output_dirs(nova_desktop)

# ---- 构建 Web 前端 (Vue + Vite) ----
# NOVA_NPM 由 cmake/environment.cmake 提供
set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../web")
set(WEB_DIST_DIR "${WEB_SRC_DIR}/dist")

# npm install (仅当 node_modules 不存在时)
if(NOT EXISTS "${WEB_SRC_DIR}/node_modules")
    message(STATUS "Running npm install in ${WEB_SRC_DIR}...")
    execute_process(
        COMMAND ${NOVA_NPM} install
        WORKING_DIRECTORY "${WEB_SRC_DIR}"
        RESULT_VARIABLE npm_install_result
    )
    if(NOT npm_install_result EQUAL 0)
        message(FATAL_ERROR "npm install failed")
    endif()
endif()

# npm run build — 每次构建 nova_desktop 前执行
add_custom_target(web_build ALL
    COMMAND ${NOVA_NPM} run build
    WORKING_DIRECTORY "${WEB_SRC_DIR}"
    COMMENT "Building web frontend (npm run build)..."
)
add_dependencies(nova_desktop web_build)

# ---- 复制 Web 构建产物到输出目录 ----
add_custom_command(TARGET nova_desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WEB_DIST_DIR}"
        "$<TARGET_FILE_DIR:nova_desktop>/web"
    COMMENT "Copying web build output to output..."
)
