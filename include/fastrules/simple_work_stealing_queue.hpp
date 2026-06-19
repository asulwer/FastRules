/**
 * @file simple_work_stealing_queue.hpp
 * @brief Simple work-stealing queue for thread pool
 * 
 * A simpler implementation of a work-stealing queue that avoids complex atomic operations.
 */

#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>

namespace fastrules {

/**
 * @brief Simple work-stealing queue
 * 
 * Uses mutex-based synchronization for simplicity and reliability.
 * Each thread has a local queue, and when idle, threads can steal work.
 * 
 * @tparam T Type of elements stored in queue
 */
template<typename T>
class SimpleWorkStealingQueue {
private:
    std::deque<T> queue_;
    mutable std::mutex mutex_;

public:
    /**
     * @brief Construct work-stealing queue
     */
    SimpleWorkStealingQueue() = default;
    
    /**
     * @brief Push item to back of queue (owner only)
     * 
     * @param item Item to push
     */
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
    }
    
    /**
     * @brief Pop item from back of queue (owner only)
     * 
     * @return Optional item, nullopt if queue empty
     */
    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        
        T item = std::move(queue_.back());
        queue_.pop_back();
        return item;
    }
    
    /**
     * @brief Steal item from front of queue (any thread)
     * 
     * @return Optional item, nullopt if queue empty
     */
    std::optional<T> steal() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }
    
    /**
     * @brief Check if queue is empty
     * 
     * @return true if empty, false otherwise
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief Get approximate size of queue
     * 
     * @return Approximate number of items in queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

} // namespace fastrules