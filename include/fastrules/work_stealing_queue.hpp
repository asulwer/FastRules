/**
 * @file work_stealing_queue.hpp
 * @brief Lock-free work-stealing queue for thread pool
 *
 * Implements a lock-free work-stealing queue based on the Chase-Lev deque algorithm.
 * Each thread has a local queue, and when idle, threads can "steal" work from
 * other threads' queues to improve load balancing.
 *
 * Thread Safety:
 * - Single producer (owner thread can push/pop)
 * - Multiple consumers (any thread can steal)
 *
 * Memory Ordering:
 * - push(): Release ordering on tail_
 * - pop(): Acquire ordering on head_ and tail_
 * - steal(): Acquire ordering on head_
 *
 * @warning NOT USED BY THE SHIPPING THREAD POOL, AND NOT PRODUCTION READY.
 * WorkStealingThreadPool uses SimpleWorkStealingQueue (mutex-based) instead.
 * Two unresolved problems make this class unsafe as written:
 *   1. resize() reassigns buffer_ while stealers may be reading it - a data
 *      race and potential use-after-free. A correct Chase-Lev deque retires
 *      old buffers through a reclamation scheme rather than freeing them.
 *   2. std::atomic<T> requires T to be trivially copyable, so it cannot hold
 *      the std::function<void()> tasks the pool actually queues.
 * Use SimpleWorkStealingQueue unless and until those are addressed.
 */

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

namespace fastrules {

/**
 * @brief Lock-free work-stealing queue
 * 
 * Chase-Lev deque implementation with:
 * - Single-producer, multiple-consumer
 * - Owner can push/pop from back (LIFO)
 * - Stealers pop from front (FIFO)
 * 
 * @tparam T Type of elements stored in queue
 */
template<typename T>
class WorkStealingQueue {
    static_assert(std::is_trivially_copyable_v<T>,
                  "WorkStealingQueue stores elements in std::atomic<T>, which requires a "
                  "trivially copyable T. For task types such as std::function<void()>, use "
                  "SimpleWorkStealingQueue instead.");

private:
    static constexpr size_t kInitialSize = 1024;

    std::vector<std::atomic<T>> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    size_t mask_;

    // NOTE: unsafe in the presence of concurrent steal() - see the file-level
    // warning. Only reachable from the owner thread's push().
    void resize() {
        size_t old_size = buffer_.size();
        size_t new_size = old_size * 2;
        
        std::vector<std::atomic<T>> new_buffer(new_size);
        for (size_t i = 0; i < new_size; ++i) {
            new_buffer[i].store(T{}, std::memory_order_relaxed);
        }
        
        for (size_t i = head_.load(std::memory_order_acquire); 
             i < tail_.load(std::memory_order_acquire); ++i) {
            new_buffer[i & (new_size - 1)].store(
                buffer_[i & (old_size - 1)].load(std::memory_order_relaxed),
                std::memory_order_relaxed
            );
        }
        
        buffer_ = std::move(new_buffer);
        mask_ = new_size - 1;
    }

public:
    /**
     * @brief Construct work-stealing queue
     */
    WorkStealingQueue() 
        : buffer_(kInitialSize)
        , head_(0)
        , tail_(0)
        , mask_(kInitialSize - 1) {
        for (size_t i = 0; i < kInitialSize; ++i) {
            buffer_[i].store(T{}, std::memory_order_relaxed);
        }
    }
    
    /**
     * @brief Push item to back of queue (owner only)
     * 
     * @param item Item to push
     */
    void push(T item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t head = head_.load(std::memory_order_acquire);
        
        if (tail - head >= buffer_.size() - 1) {
            resize();
        }
        
        buffer_[tail & mask_].store(item, std::memory_order_relaxed);
        tail_.store(tail + 1, std::memory_order_release);
    }
    
    /**
     * @brief Pop item from back of queue (owner only)
     * 
     * @return Optional item, nullopt if queue empty
     */
    std::optional<T> pop() {
        size_t tail = tail_.load(std::memory_order_acquire);
        if (tail == 0) return std::nullopt;
        
        tail_.store(tail - 1, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_acquire);
        
        T item = buffer_[(tail - 1) & mask_].load(std::memory_order_relaxed);
        
        size_t head = head_.load(std::memory_order_relaxed);
        if (tail - 1 > head) {
            return item;
        }
        
        if (tail - 1 == head) {
            if (head_.compare_exchange_strong(head, tail, 
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
                tail_.store(tail, std::memory_order_release);
                return item;
            } else {
                tail_.store(tail, std::memory_order_release);
                return std::nullopt;
            }
        }
        
        tail_.store(tail, std::memory_order_release);
        return std::nullopt;
    }
    
    /**
     * @brief Steal item from front of queue (any thread)
     * 
     * @return Optional item, nullopt if queue empty
     */
    std::optional<T> steal() {
        size_t head = head_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        
        if (tail <= head) return std::nullopt;
        
        T item = buffer_[head & mask_].load(std::memory_order_relaxed);
        
        if (!head_.compare_exchange_strong(head, head + 1,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
            return std::nullopt;
        }
        
        return item;
    }
    
    /**
     * @brief Check if queue is empty
     * 
     * @return true if empty, false otherwise
     */
    bool empty() const {
        return head_.load(std::memory_order_acquire) >= 
               tail_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Get approximate size of queue
     * 
     * @return Approximate number of items in queue
     */
    size_t size() const {
        size_t tail = tail_.load(std::memory_order_acquire);
        size_t head = head_.load(std::memory_order_acquire);
        return tail > head ? tail - head : 0;
    }
};

} // namespace fastrules