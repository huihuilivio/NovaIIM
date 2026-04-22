#pragma once
// Win32UIDispatcher — 通过 PostMessage(WM_APP) 投递回调到 Win32 消息循环

#include <viewmodel/ui_dispatcher.h>

#include <Windows.h>
#include <atomic>

namespace nova::desktop {

class Win32UIDispatcher {
public:
    /// 注册到 UIDispatcher 静态接口
    static void Install();

    /// 设置目标窗口句柄（必须在 Install() 之前或之后立即调用）
    static void SetHwnd(HWND hwnd);

private:
    static std::atomic<HWND> hwnd_;
};

}  // namespace nova::desktop
