#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "mpmc_queue.h"

namespace nova {

// Worker 线程池（对应架构文档第 7 节）
// IO线程(libhv) → MPMCQueue → Worker线程池
class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t num_threads, size_t queue_capacity = 8192)
        : queue_(queue_capacity), stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { WorkerLoop(); });
        }
    }

    ~ThreadPool() {
        Stop();
    }

    // 提交任务到队列
    bool Submit(Task task) {
        if (stop_) return false;
        bool ok = queue_.Push(std::move(task));
        if (ok) {
            cv_.notify_one();
        }
        return ok;
    }

    void Stop() {
        stop_ = true;
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

private:
    void WorkerLoop() {
        while (!stop_) {
            Task task;
            if (queue_.Pop(task)) {
                task();
            } else {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait_for(lock, std::chrono::milliseconds(1));
            }
        }
        // drain remaining tasks
        Task task;
        while (queue_.Pop(task)) {
            task();
        }
    }

    MPMCQueue<Task>          queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool>        stop_;
    std::mutex               mutex_;
    std::condition_variable  cv_;
};

} // namespace nova
