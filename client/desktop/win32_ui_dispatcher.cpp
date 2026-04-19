#include "win32_ui_dispatcher.h"

namespace nova::desktop {

HWND Win32UIDispatcher::hwnd_ = nullptr;

void Win32UIDispatcher::SetHwnd(HWND hwnd) {
    hwnd_ = hwnd;
}

void Win32UIDispatcher::Install() {
    nova::client::UIDispatcher::Set([](std::function<void()> fn) {
        if (hwnd_) {
            // 通过堆分配传递回调，WM_APP 处理时 delete
            auto* p = new std::function<void()>(std::move(fn));
            PostMessage(hwnd_, WM_APP, 0, reinterpret_cast<LPARAM>(p));
        }
    });
}

}  // namespace nova::desktop
