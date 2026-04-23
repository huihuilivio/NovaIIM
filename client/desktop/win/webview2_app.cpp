#include "webview2_app.h"
#include "js_bridge.h"
#include "tray_icon.h"
#include "win32_ui_dispatcher.h"

#include <viewmodel/nova_client.h>

#include <infra/logger.h>

#include <ShlObj.h>
#include <wrl/event.h>

#include <atomic>

using namespace Microsoft::WRL;

namespace nova::desktop {

static std::atomic<WebView2App*> g_app{nullptr};

// ---- 构造 / 析构 ----

WebView2App::WebView2App(HINSTANCE hInstance, nova::client::NovaClient* client)
    : hinstance_(hInstance), client_(client), tray_(std::make_unique<TrayIcon>()) {
    g_app.store(this, std::memory_order_release);
}

WebView2App::~WebView2App() {
    if (bridge_) bridge_.reset();
    if (tray_) tray_->Destroy();
    g_app.store(nullptr, std::memory_order_release);
}

// ---- 初始化 ----

bool WebView2App::Init(int nCmdShow) {
    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hinstance_;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(hinstance_, L"IDI_APP_ICON");
    wc.hIconSm       = LoadIconW(hinstance_, L"IDI_APP_ICON");
    wc.hbrBackground = CreateSolidBrush(RGB(0x1D, 0x1E, 0x1F));   // 深色背景避免白闪
    wc.lpszClassName = L"NovaIIMDesktop";
    RegisterClassExW(&wc);

    // 创建窗口
    hwnd_ = CreateWindowExW(
        0, L"NovaIIMDesktop", L"NovaIIM",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 720,
        nullptr, nullptr, hinstance_, nullptr
    );
    if (!hwnd_) return false;

    // 安装 UI 线程投递器
    Win32UIDispatcher::SetHwnd(hwnd_);
    Win32UIDispatcher::Install();

    // 启动 WebView2 异步初始化
    InitWebView2();

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

int WebView2App::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

// ---- 窗口消息处理 ----

LRESULT CALLBACK WebView2App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* app = g_app.load(std::memory_order_acquire);
    if (app && app->hwnd_ == hwnd) {
        return app->HandleMessage(msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT WebView2App::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            // 最小化时隐藏窗口到托盘
            ShowWindow(hwnd_, SW_HIDE);
            return 0;
        }
        Resize();
        return 0;

    case WM_CLOSE:
        // 关闭按钮最小化到托盘而不是退出
        ShowWindow(hwnd_, SW_HIDE);
        return 0;

    case WM_DESTROY:
        // 销毁 bridge 先于 client.Shutdown()，防止悬挂回调
        bridge_.reset();
        tray_->Destroy();
        Win32UIDispatcher::SetHwnd(nullptr);
        PostQuitMessage(0);
        return 0;

    case WM_APP: {
        // UIDispatcher 回调
        auto* fn = reinterpret_cast<std::function<void()>*>(lp);
        if (fn) {
            if (*fn) (*fn)();
            delete fn;
        }
        return 0;
    }

    case WM_TRAYICON:
        // 托盘图标消息
        if (tray_ && tray_->HandleMessage(hwnd_, lp)) return 0;
        break;

    default:
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }
}

// ---- WebView2 初始化 ----

void WebView2App::InitWebView2() {
    // 使用 %LOCALAPPDATA%\NovaIIM\WebView2Data 持久化 localStorage 等数据
    wchar_t app_data[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, app_data);
    std::wstring user_data_folder = std::wstring(app_data) + L"\\NovaIIM\\WebView2Data";

    auto hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, user_data_folder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) {
                    NOVA_LOG_ERROR("Failed to create WebView2 environment: 0x{:08X}", result);
                    PostQuitMessage(1);
                    return result;
                }
                env_ = env;
                env_->CreateCoreWebView2Controller(
                    hwnd_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result)) {
                                NOVA_LOG_ERROR("Failed to create WebView2 controller: 0x{:08X}", result);
                                PostQuitMessage(1);
                                return result;
                            }
                            controller_ = controller;
                            HRESULT cwhr = controller_->get_CoreWebView2(&webview_);
                            if (FAILED(cwhr) || !webview_) {
                                NOVA_LOG_ERROR("get_CoreWebView2 failed: 0x{:08X}", static_cast<unsigned>(cwhr));
                                PostQuitMessage(1);
                                return cwhr;
                            }
                            OnWebViewReady();
                            return S_OK;
                        }).Get()
                );
                return S_OK;
            }).Get()
    );

    if (FAILED(hr)) {
        NOVA_LOG_ERROR("CreateCoreWebView2EnvironmentWithOptions failed: 0x{:08X}", hr);
        MessageBoxW(hwnd_,
            L"WebView2 Runtime 未安装。\n请先安装 Microsoft Edge WebView2 Runtime。",
            L"NovaIIM", MB_ICONERROR);
        PostQuitMessage(1);
    }
}

void WebView2App::OnWebViewReady() {
    // 基础设置
    ComPtr<ICoreWebView2Settings> settings;
    webview_->get_Settings(&settings);
    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);

#ifdef NDEBUG
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
#endif

    // 建立 JS Bridge
    bridge_ = std::make_unique<JsBridge>(webview_.Get(), client_, this);
    bridge_->Init();

    // 导航白名单：拒绝除 novaim.local（本地虚拟主机）与 about:blank 之外的任何导航，
    // 防御 XSS / RCE 通过诱导导航到外部恶意页面注入钓鱼/木马。
    EventRegistrationToken nav_token;
    webview_->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                LPWSTR uri = nullptr;
                if (FAILED(args->get_Uri(&uri)) || !uri) return S_OK;
                std::wstring wuri(uri);
                CoTaskMemFree(uri);
                // 允许：https://novaim.local/..., about:blank, about:srcdoc
                bool allowed =
                    wuri.rfind(L"https://novaim.local/", 0) == 0 ||
                    wuri == L"about:blank" ||
                    wuri.rfind(L"about:srcdoc", 0) == 0;
                if (!allowed) {
                    NOVA_LOG_WARN("navigation blocked: non-whitelisted URI");
                    args->put_Cancel(TRUE);
                }
                return S_OK;
            }).Get(),
        &nav_token);

    // 新窗口拦截：禁止弹出窗口，防止页面逃逸
    EventRegistrationToken newwin_token;
    webview_->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);  // 吞掉请求，不打开新窗口
                return S_OK;
            }).Get(),
        &newwin_token);

    // 在文档解析前注入 CSP 元标签（纵深防御；真正的 CSP 应由服务端 header 下发，
    // 这里作为对本地虚拟主机静态资源的兜底防御）。
    static const wchar_t kCspInject[] =
        L"(function(){try{var m=document.createElement('meta');"
        L"m.httpEquiv='Content-Security-Policy';"
        L"m.content=\"default-src 'self' https://novaim.local;"
        L" script-src 'self' 'unsafe-inline' https://novaim.local;"
        L" style-src 'self' 'unsafe-inline' https://novaim.local;"
        L" img-src 'self' data: blob: https://novaim.local;"
        L" connect-src 'self' https://novaim.local ws://localhost:* wss://novaim.local;"
        L" frame-ancestors 'none';"
        L" object-src 'none';"
        L" base-uri 'self'\";"
        L"(document.head||document.documentElement).appendChild(m);}catch(e){}})();";
    webview_->AddScriptToExecuteOnDocumentCreated(kCspInject, nullptr);

    // 将本地 web/ 目录映射为虚拟主机
    ComPtr<ICoreWebView2_3> webview3;
    webview_->QueryInterface(IID_PPV_ARGS(&webview3));
    if (webview3) {
        auto webDir = GetWebDir();
        HRESULT mhr = webview3->SetVirtualHostNameToFolderMapping(
            L"novaim.local", webDir.c_str(),
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
        );
        if (FAILED(mhr)) {
            NOVA_LOG_ERROR("SetVirtualHostNameToFolderMapping failed: 0x{:08X}", static_cast<unsigned>(mhr));
        }
    } else {
        NOVA_LOG_WARN("ICoreWebView2_3 not available; virtual host mapping skipped");
    }

    // 导航到主页
    webview_->Navigate(L"https://novaim.local/index.html");

    // 填充窗口
    Resize();

    // 创建托盘图标
    tray_->Create(hwnd_, hinstance_);
    tray_->SetQuitCallback([this]() {
        if (client_) client_->Shutdown();
    });

    NOVA_LOG_INFO("WebView2 initialized successfully");
}

// ---- 布局 ----

void WebView2App::Resize() {
    if (!controller_) return;
    RECT bounds;
    GetClientRect(hwnd_, &bounds);
    controller_->put_Bounds(bounds);
}

std::wstring WebView2App::GetWebDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path);
    auto pos = dir.find_last_of(L'\\');
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos);
    }
    return dir + L"\\web";
}

// ---- 托盘通知 ----

void WebView2App::ShowTrayBalloon(const std::wstring& title, const std::wstring& message) {
    if (tray_) tray_->ShowBalloon(title, message);
}

void WebView2App::FlashTaskbar() {
    if (tray_ && hwnd_) tray_->FlashTaskbar(hwnd_);
}

}  // namespace nova::desktop
