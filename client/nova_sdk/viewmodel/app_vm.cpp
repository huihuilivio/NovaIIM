#include "app_vm.h"
#include <core/client_context.h>
#include <nova/protocol.h>

namespace nova::client {

AppVM::AppVM(ClientContext& ctx) : ctx_(ctx) {
    // 订阅 ClientContext 状态变更 → 驱动 Observable
    state_.Set(ctx_.GetState());
    ctx_.OnStateChanged([this](ClientState s) {
        state_.Set(s);
    });

    // 订阅踢下线通知
    kick_sub_id_ = ctx_.Events().subscribe<nova::proto::KickNotify>("KickNotify",
        [this](const nova::proto::KickNotify& n) {
            if (kick_cb_) kick_cb_(n.code, n.msg);
        });
}
AppVM::~AppVM() {
    if (kick_sub_id_) {
        ctx_.Events().unsubscribe(kick_sub_id_);
    }
}

}  // namespace nova::client
