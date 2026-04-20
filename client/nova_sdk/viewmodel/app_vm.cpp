#include "app_vm.h"
#include <core/client_context.h>

namespace nova::client {

AppVM::AppVM(ClientContext& ctx) : ctx_(ctx) {
    // 订阅 ClientContext 状态变更 → 驱动 Observable
    state_.Set(ctx_.GetState());
    ctx_.OnStateChanged([this](ClientState s) {
        state_.Set(s);
    });
}
AppVM::~AppVM() = default;

}  // namespace nova::client
