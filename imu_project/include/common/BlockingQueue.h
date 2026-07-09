#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

namespace imu {

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {}

    bool push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || capacity_ == 0) {
            return false;
        }

        if (queue_.size() >= capacity_) {
            queue_.pop();
        }

        queue_.push(std::move(value));
        condition_.notify_one();
        return true;
    }

    bool waitPop(T& value, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait_for(lock, timeout, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool tryPop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        condition_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<T> queue_;
    bool closed_{false};
};

}  // namespace imu
