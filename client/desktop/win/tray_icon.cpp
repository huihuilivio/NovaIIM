#include "tray_icon.h"

namespace nova::desktop {

TrayIcon::~TrayIcon() {
    Destroy();
}

bool TrayIcon::Create(HWND hwnd, HINSTANCE hinstance) {
    if (created_) return true;

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize           = sizeof(nid_);
    nid_.hWnd             = hwnd;
    nid_.uID              = 1;
    nid_.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;

    // 使用应用图标，如果没有则使用默认
    nid_.hIcon = LoadIconW(hinstance, L"IDI_APP_ICON");
    if (!nid_.hIcon) {
        nid_.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));  // IDI_APPLICATION
    }

    wcscpy_s(nid_.szTip, L"NovaIIM");

    created_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    return created_;
}

void TrayIcon::Destroy() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& message) {
    if (!created_) return;

    nid_.uFlags = NIF_INFO;
    nid_.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;

    // 安全复制标题 (max 63 chars)
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    // 安全复制消息 (max 255 chars)
    wcsncpy_s(nid_.szInfo, message.c_str(), _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &nid_);

    // 恢复 flags
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void TrayIcon::FlashTaskbar(HWND hwnd, int count) {
    FLASHWINFO fwi = {};
    fwi.cbSize    = sizeof(fwi);
    fwi.hwnd      = hwnd;
    fwi.dwFlags   = FLASHW_ALL | FLASHW_TIMERNOFG;
    fwi.uCount    = static_cast<UINT>(count);
    fwi.dwTimeout = 0;  // 使用系统默认闪烁间隔
    FlashWindowEx(&fwi);
}

bool TrayIcon::HandleMessage(HWND hwnd, LPARAM lp) {
    switch (LOWORD(lp)) {
    case WM_LBUTTONUP:
        // 左键单击: 恢复窗口
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        return true;

    case WM_RBUTTONUP:
        // 右键单击: 显示菜单
        ShowContextMenu(hwnd);
        return true;

    case NIN_BALLOONUSERCLICK:
        // 点击气泡通知: 恢复窗口
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        return true;
    }
    return false;
}

void TrayIcon::ShowContextMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"显示主窗口");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);

    // 必须调用 SetForegroundWindow 否则菜单不会自动关闭
    SetForegroundWindow(hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                               pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    if (cmd == ID_TRAY_SHOW) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    } else if (cmd == ID_TRAY_QUIT) {
        if (quit_callback_) {
            quit_callback_();
        }
        DestroyWindow(hwnd);
    }
}

}  // namespace nova::desktop
