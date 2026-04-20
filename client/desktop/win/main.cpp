// NovaIIM Desktop — Win32 + WebView2 入口

#include "webview2_app.h"
#include "win32_ui_dispatcher.h"

#include <viewmodel/nova_client.h>

#include <Windows.h>
#include <objbase.h>

#include <string>

static std::string GetExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir(path);
    auto pos = dir.find_last_of('\\');
    if (pos != std::string::npos) dir = dir.substr(0, pos);
    return dir;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*prev*/, LPWSTR /*cmdLine*/, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    auto config_path = GetExeDir() + "\\config.yaml";

    nova::client::NovaClient client(config_path);
    client.Init();

    nova::desktop::WebView2App app(hInstance, &client);
    if (!app.Init(nCmdShow)) {
        client.Shutdown();
        CoUninitialize();
        return -1;
    }

    int ret = app.Run();
    client.Shutdown();
    CoUninitialize();
    return ret;
}
