#pragma once
// RequestManager — seq_id 请求-响应匹配
//
// 每个请求分配唯一 seq_id，注册超时回调
// 收到应答后按 seq 匹配并调用回调

#include <core/export.h>

#include <nova/packet.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace nova::client {

class NOVA_CLIENT_API RequestManager {
public:
    using ResponseCallback = std::function<void(const nova::proto::Packet& resp)>;
    using TimeoutCallback  = std::function<void(uint32_t seq)>;

    explicit RequestManager(uint32_t default_timeout_ms = 10000);
    ~RequestManager() = default;

    /// 注册一个待应答请求
    void AddPending(uint32_t seq, ResponseCallback on_resp,
                    TimeoutCallback on_timeout = nullptr);

    /// 收到应答时调用（匹配 seq 并触发回调）
    /// @return true 如果 seq 匹配到了注册的请求
    bool HandleResponse(const nova::proto::Packet& resp);

    /// 检查超时（由定时器周期调用）
    void CheckTimeouts();

    /// 取消一个待应答请求
    void Cancel(uint32_t seq);

    /// 取消所有待应答
    void CancelAll();

    /// 当前待应答数
    size_t PendingCount() const;

private:
    struct PendingRequest {
        ResponseCallback on_resp;
        TimeoutCallback  on_timeout;
        std::chrono::steady_clock::time_point deadline;
    };

    uint32_t default_timeout_ms_;
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, PendingRequest> pending_;
};

}  // namespace nova::client
