#pragma once
// AppVM — 应用状态 ViewModel（连接状态观察）

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <cstdint>
#include <functional>
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
};

}  // namespace nova::client
