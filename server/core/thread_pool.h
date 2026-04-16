#pragma once

#include <cstdio>
#include <functional>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <vector>
#include "mpmc_queue.h"

namespace nova {

// Worker 线程池（对应架构文档第 7 节）
// IO线程(libhv) → MPMCQueue → Worker线程池
// 使用 counting_semaphore 替代 mutex+CV：每次 Push 精确唤醒一个 worker，
// 空闲时零 CPU 消耗，无 lost-wakeup 风险
class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t num_threads, size_t queue_capacity = 8192)
        : queue_(queue_capacity), stop_(false), sem_(0) {
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
            sem_.release();
        }
        return ok;
    }

    void Stop() {
        if (stop_.exchange(true)) return;  // 重入安全
        // 唤醒所有等待中的 worker
        for (size_t i = 0; i < workers_.size(); ++i) {
            sem_.release();
        }
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

private:
    void WorkerLoop() {
        while (true) {
            sem_.acquire();
            if (stop_) break;
            Task task;
            if (queue_.Pop(task)) {
                try {
                    task();
                } catch (const std::exception& e) {
                    fprintf(stderr, "[ThreadPool] unhandled exception: %s\n", e.what());
                } catch (...) {
                    fprintf(stderr, "[ThreadPool] unknown exception\n");
                }
            }
        }
        // drain remaining tasks
        Task task;
        while (queue_.Pop(task)) {
            try {
                task();
            } catch (...) {}
        }
    }

    MPMCQueue<Task>          queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool>        stop_;
    std::counting_semaphore<65536> sem_;
};

} // namespace nova
