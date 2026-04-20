#include "request_manager.h"
#include <infra/logger.h>

namespace nova::client {

RequestManager::RequestManager(uint32_t default_timeout_ms)
    : default_timeout_ms_(default_timeout_ms) {}

void RequestManager::AddPending(uint32_t seq, ResponseCallback on_resp,
                                TimeoutCallback on_timeout) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(default_timeout_ms_);
    std::lock_guard lock(mutex_);
    pending_[seq] = PendingRequest{std::move(on_resp), std::move(on_timeout), deadline};
}

bool RequestManager::HandleResponse(const nova::proto::Packet& resp) {
    PendingRequest req;
    {
        std::lock_guard lock(mutex_);
        auto it = pending_.find(resp.seq);
        if (it == pending_.end()) return false;
        req = std::move(it->second);
        pending_.erase(it);
    }
    if (req.on_resp) {
        req.on_resp(resp);
    }
    return true;
}

void RequestManager::CheckTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint32_t, TimeoutCallback>> timed_out;
    {
        std::lock_guard lock(mutex_);
        for (auto it = pending_.begin(); it != pending_.end();) {
            if (now >= it->second.deadline) {
                timed_out.emplace_back(it->first, std::move(it->second.on_timeout));
                it = pending_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& [seq, cb] : timed_out) {
        NOVA_LOG_WARN("Request seq={} timed out", seq);
        if (cb) cb(seq);
    }
}

void RequestManager::Cancel(uint32_t seq) {
    std::lock_guard lock(mutex_);
    pending_.erase(seq);
}

void RequestManager::CancelAll() {
    std::lock_guard lock(mutex_);
    pending_.clear();
}

size_t RequestManager::PendingCount() const {
    std::lock_guard lock(mutex_);
    return pending_.size();
}

}  // namespace nova::client
