#pragma once
// AppVM — 应用状态 ViewModel（连接状态观察）

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

namespace nova::client {

class ClientContext;

class NOVA_SDK_API AppVM {
public:
    explicit AppVM(ClientContext& ctx);
    ~AppVM();

    /// 连接状态（Observable — View 通过 Observe 订阅变更）
    Observable<ClientState>& State() { return state_; }

private:
    ClientContext& ctx_;
    Observable<ClientState> state_{ClientState::kDisconnected};
};

}  // namespace nova::client
