#include "app_vm.h"
#include <core/client_context.h>
#include <nova/protocol.h>

namespace nova::client {

AppVM::AppVM(ClientContext& ctx) : ctx_(ctx) {
    // 订阅 ClientContext 状态变更 → 驱动 Observable
    state_.Set(ctx_.GetState());
    ctx_.OnStateChanged([this, alive = alive_](ClientState s) {
        if (!alive->load()) return;  // AppVM 已销毁
        state_.Set(s);
    });

    // 订阅踢下线通知
    kick_sub_id_ = ctx_.Events().subscribe<nova::proto::KickNotify>("KickNotify",
        [this, alive = alive_](const nova::proto::KickNotify& n) {
            if (!alive->load()) return;
            if (kick_cb_) kick_cb_(n.code, n.msg);
        });
}
AppVM::~AppVM() {
    alive_->store(false);
    if (kick_sub_id_) {
        ctx_.Events().unsubscribe(kick_sub_id_);
    }
}

}  // namespace nova::client
