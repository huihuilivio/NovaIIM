// NovaIIM Desktop — Win32 + WebView2 入口

#include "webview2_app.h"
#include "win32_ui_dispatcher.h"

#include <core/client_config.h>
#include <core/client_context.h>
#include <core/logger.h>

#include <Windows.h>
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*prev*/, LPWSTR /*cmdLine*/, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // 客户端配置
    nova::client::ClientConfig config;
    config.server_host = "127.0.0.1";
    config.server_port = 9090;
    config.device_type = "pc";
    config.log_level   = "debug";

    // 初始化客户端上下文
    nova::client::ClientContext ctx(config);
    ctx.Init();

    // 创建并运行 WebView2 应用
    nova::desktop::WebView2App app(hInstance, &ctx);
    if (!app.Init(nCmdShow)) {
        ctx.Shutdown();
        CoUninitialize();
        return -1;
    }

    int ret = app.Run();
    ctx.Shutdown();
    CoUninitialize();
    return ret;
}
