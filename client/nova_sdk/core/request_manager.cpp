#include "request_manager.h"
#include <infra/logger.h>

namespace nova::client {

RequestManager::RequestManager(uint32_t default_timeout_ms)
    : default_timeout_ms_(default_timeout_ms) {}

void RequestManager::AddPending(uint32_t seq, ResponseCallback on_resp,
                                TimeoutCallback on_timeout) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(default_timeout_ms_);
    PendingRequest evicted;
    bool had_evicted = false;
    {
        std::lock_guard lock(mutex_);
        auto it = pending_.find(seq);
        if (it != pending_.end()) {
            // seq_id 冲突（理论上仅在 ~2^32 次请求后回绕时可能出现）。
            // 取出旧请求在锁外触发其 timeout 回调，避免静默丢失。
            evicted = std::move(it->second);
            had_evicted = true;
            pending_.erase(it);
        }
        pending_[seq] = PendingRequest{std::move(on_resp), std::move(on_timeout), deadline};
    }
    if (had_evicted) {
        NOVA_LOG_WARN("RequestManager: seq={} reused while still pending; evicting old request", seq);
        if (evicted.on_timeout) evicted.on_timeout(seq);
    }
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
