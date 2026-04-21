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
    tray_icon.h
    tray_icon.cpp
    app.rc
)

target_link_libraries(nova_desktop PRIVATE
    nova_sdk
    webview2_sdk
)

nova_set_compiler_options(nova_desktop)
nova_set_output_dirs(nova_desktop)

# ---- 构建 Desktop Web 前端 ----
nova_add_web_frontend(
    NAME       desktop_web_build
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../web"
    OUTPUT_DIR web
    DEPENDS    nova_desktop
)
