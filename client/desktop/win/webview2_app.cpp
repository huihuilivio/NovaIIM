#include "webview2_app.h"
#include "js_bridge.h"
#include "win32_ui_dispatcher.h"

#include <infra/logger.h>

#include <ShlObj.h>
#include <wrl/event.h>

using namespace Microsoft::WRL;

namespace nova::desktop {

static WebView2App* g_app = nullptr;

// ---- 构造 / 析构 ----

WebView2App::WebView2App(HINSTANCE hInstance, nova::client::NovaClient* client)
    : hinstance_(hInstance), client_(client) {
    g_app = this;
}

WebView2App::~WebView2App() {
    if (bridge_) bridge_.reset();  // 可能已在 WM_DESTROY 中释放
    g_app = nullptr;
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
    if (g_app && g_app->hwnd_ == hwnd) {
        return g_app->HandleMessage(msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT WebView2App::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        Resize();
        return 0;

    case WM_DESTROY:
        // 销毁 bridge 先于 client.Shutdown()，防止悬挂回调
        bridge_.reset();
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
                            controller_->get_CoreWebView2(&webview_);
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
    bridge_ = std::make_unique<JsBridge>(webview_.Get(), client_);
    bridge_->Init();

    // 将本地 web/ 目录映射为虚拟主机
    ComPtr<ICoreWebView2_3> webview3;
    webview_->QueryInterface(IID_PPV_ARGS(&webview3));
    if (webview3) {
        auto webDir = GetWebDir();
        webview3->SetVirtualHostNameToFolderMapping(
            L"novaim.local", webDir.c_str(),
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
        );
    }

    // 导航到主页
    webview_->Navigate(L"https://novaim.local/index.html");

    // 填充窗口
    Resize();

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

}  // namespace nova::desktop
