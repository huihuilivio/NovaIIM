#pragma once
// AppVM — 应用状态 ViewModel（连接状态观察）

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class ClientContext;

class NOVA_SDK_API AppVM {
public:
    explicit AppVM(ClientContext& ctx);
    ~AppVM();

    /// 连接状态（Observable — View 通过 Observe 订阅变更）
    Observable<ClientState>& State() { return state_; }

    /// 被踢下线回调
    using KickCallback = std::function<void(int reason, const std::string& msg)>;
    void OnKicked(KickCallback cb) { kick_cb_ = std::move(cb); }

private:
    ClientContext& ctx_;
    Observable<ClientState> state_{ClientState::kDisconnected};
    KickCallback kick_cb_;
    uint64_t kick_sub_id_{0};
    // 生命周期守卫： ClientContext::state_callbacks_ 不支持 unsubscribe，
    // 需在析构后拦截 state 回调以免访问已销毁的 this。
    std::shared_ptr<std::atomic<bool>> alive_{std::make_shared<std::atomic<bool>>(true)};
};

}  // namespace nova::client
