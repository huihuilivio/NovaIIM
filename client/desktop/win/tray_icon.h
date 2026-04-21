#pragma once
// TrayIcon — Win32 系统托盘图标管理
//
// 功能: 托盘图标 / 气泡通知 / 左键单击恢复 / 右键菜单退出
// 集成: 最小化时隐藏窗口到托盘, 收到新消息时闪烁任务栏 + 气泡提示

#include <Windows.h>
#include <shellapi.h>

#include <functional>
#include <string>

namespace nova::desktop {

// 自定义消息 ID
constexpr UINT WM_TRAYICON = WM_APP + 100;

// 右键菜单项 ID
constexpr UINT ID_TRAY_SHOW   = 40001;
constexpr UINT ID_TRAY_QUIT   = 40002;

class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    /// 创建托盘图标 (在 WM_CREATE 或 OnWebViewReady 后调用)
    bool Create(HWND hwnd, HINSTANCE hinstance);

    /// 移除托盘图标 (在 WM_DESTROY 之前调用)
    void Destroy();

    /// 显示气泡通知
    void ShowBalloon(const std::wstring& title, const std::wstring& message);

    /// 闪烁任务栏按钮
    void FlashTaskbar(HWND hwnd, int count = 3);

    /// 处理 WM_TRAYICON 消息, 返回 true 表示已处理
    bool HandleMessage(HWND hwnd, LPARAM lp);

    /// 设置退出回调
    void SetQuitCallback(std::function<void()> cb) { quit_callback_ = std::move(cb); }

private:
    void ShowContextMenu(HWND hwnd);

    NOTIFYICONDATAW nid_ = {};
    bool created_ = false;
    std::function<void()> quit_callback_;
};

}  // namespace nova::desktop
