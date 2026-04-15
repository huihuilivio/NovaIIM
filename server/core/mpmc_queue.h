#pragma once

#include <atomic>
#include <cassert>
#include <vector>

namespace nova {

// Vyukov MPMC 无锁有界队列（对应架构文档第 8 节）
// 用途：消息分发、IO 线程 → Worker 线程池
template <typename T>
class MPMCQueue {
public:
    explicit MPMCQueue(size_t capacity)
        : buffer_(capacity), capacity_(capacity), mask_(capacity - 1) {
        assert(capacity >= 2 && (capacity & (capacity - 1)) == 0
               && "MPMCQueue capacity must be a power of 2");
        for (size_t i = 0; i < capacity; ++i) {
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    // 非阻塞入队（拷贝），成功返回 true
    bool Push(const T& item) {
        return PushImpl(item);
    }

    // 非阻塞入队（移动），成功返回 true
    bool Push(T&& item) {
        return PushImpl(std::move(item));
    }

    // 非阻塞出队，成功返回 true
    bool Pop(T& item) {
        Cell* cell;
        size_t pos = tail_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // 队列空
            } else {
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
        item = std::move(cell->data);
        cell->seq.store(pos + mask_ + 1, std::memory_order_release);
        return true;
    }

private:
    template <typename U>
    bool PushImpl(U&& item) {
        Cell* cell;
        size_t pos = head_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // 队列满
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
        cell->data = std::forward<U>(item);
        cell->seq.store(pos + 1, std::memory_order_release);
        return true;
    }

    struct Cell {
        std::atomic<size_t> seq;
        T data;
    };

    std::vector<Cell> buffer_;
    size_t capacity_;
    size_t mask_;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)  // 对齐填充是预期行为
#endif
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

} // namespace nova
