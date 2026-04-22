#include "win32_ui_dispatcher.h"

namespace nova::desktop {

std::atomic<HWND> Win32UIDispatcher::hwnd_{nullptr};

void Win32UIDispatcher::SetHwnd(HWND hwnd) {
    hwnd_.store(hwnd, std::memory_order_release);
}

void Win32UIDispatcher::Install() {
    nova::client::UIDispatcher::Set([](std::function<void()> fn) {
        HWND h = hwnd_.load(std::memory_order_acquire);
        if (!h) return;
        // 通过堆分配传递回调，WM_APP 处理时 delete
        auto* p = new std::function<void()>(std::move(fn));
        if (!PostMessage(h, WM_APP, 0, reinterpret_cast<LPARAM>(p))) {
            delete p;  // PostMessage 失败时防止内存泄漏
        }
    });
}

}  // namespace nova::desktop
