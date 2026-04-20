#pragma once
// WebView2App — Win32 窗口 + WebView2 生命周期管理

#include <Windows.h>
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>

#include <memory>
#include <string>

namespace nova::client { class NovaClient; }

namespace nova::desktop {

class JsBridge;

class WebView2App {
public:
    WebView2App(HINSTANCE hInstance, nova::client::NovaClient* client);
    ~WebView2App();

    WebView2App(const WebView2App&) = delete;
    WebView2App& operator=(const WebView2App&) = delete;

    /// 初始化窗口 + WebView2
    bool Init(int nCmdShow);

    /// 运行消息循环
    int Run();

    HWND GetHwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void InitWebView2();
    void OnWebViewReady();
    void Resize();
    std::wstring GetWebDir();

    HINSTANCE hinstance_ = nullptr;
    HWND hwnd_ = nullptr;
    nova::client::NovaClient* client_ = nullptr;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> env_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

    std::unique_ptr<JsBridge> bridge_;
};

}  // namespace nova::desktop
