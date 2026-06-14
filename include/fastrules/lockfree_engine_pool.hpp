#pragma once

#include "fastrules/logger.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <stack>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <algorithm>

// Platform-specific intrinsics for spin-wait
#if defined(_MSC_VER)
    #include <intrin.h>  // For _mm_pause on MSVC
#elif defined(__x86_64__) || defined(__i386__)
    // GCC/Clang inline assembly
#endif

namespace fastrules {

// Forward declaration
class LuaEngine;

/**
 * @brief Thread-safe engine pool with exponential backoff wait strategy
 * 
 * Fast path: Try to acquire mutex without blocking
 * Slow path: Adaptive spinning with exponential backoff
 * 
 * This provides good performance under low contention while
 * avoiding the livelock issues of pure lock-free algorithms.
 */
class LockFreeEnginePool {
public:
    LockFreeEnginePool() = default;
    
    ~LockFreeEnginePool() = default;

    /**
     * @brief Push an engine onto the pool
     * 
     * Thread-safe, wakes up waiting threads.
     */
    void push(LuaEngine* engine) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stack_.push(engine);
        }
        cv_.notify_one();
    }

    /**
     * @brief Pop an engine from the pool
     * 
     * Thread-safe: returns immediately if available, nullptr if empty.
     * Non-blocking - use tryPop() for blocking behavior.
     * 
     * @return Pointer to LuaEngine, or nullptr if pool is empty
     */
    LuaEngine* pop() {
        // Fast path: try non-blocking pop
        {
            std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
            if (lock.owns_lock() && !stack_.empty()) {
                LuaEngine* engine = stack_.top();
                stack_.pop();
                return engine;
            }
        }
        return nullptr;
    }

    /**
     * @brief Try to pop with timeout using exponential backoff
     * 
     * Adaptive strategy:
     * 1. Spin with yield (fast, low latency)
     * 2. Exponential backoff with sleep (medium latency)
     * 3. Block with condition variable (low CPU)
     * 
     * Blocks until engine available or timeout.
     * Used by acquireEngine() in workflow.cpp
     */
    LuaEngine* tryPop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        auto start = std::chrono::steady_clock::now();
        
        // Phase 1: Fast spinning with yield (first ~10% of timeout)
        auto spinEnd = start + timeout / 10;
        int spinCount = 0;
        while (std::chrono::steady_clock::now() < spinEnd) {
            if (LuaEngine* engine = tryPopFast()) {
                return engine;
            }
            // Adaptive spin: pause then yield
            if (++spinCount % 16 == 0) {
                std::this_thread::yield();
            } else {
                #if defined(_MSC_VER)
                    _mm_pause();
                #elif defined(__x86_64__) || defined(__i386__)
                    __asm__ volatile("pause" ::: "memory");
                #else
                    std::this_thread::yield();
                #endif
            }
        }
        
        // Phase 2: Exponential backoff with short sleeps (next ~20% of timeout)
        auto backoffEnd = start + timeout / 5;
        std::chrono::microseconds sleepTime(1);
        while (std::chrono::steady_clock::now() < backoffEnd) {
            if (LuaEngine* engine = tryPopFast()) {
                return engine;
            }
            std::this_thread::sleep_for(sleepTime);
            // Cap at 1ms max sleep
            if (sleepTime < std::chrono::milliseconds(1)) {
                sleepTime *= 2;
            }
        }
        
        // Phase 3: Block with condition variable (remaining time)
        auto remaining = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (remaining.count() <= 0) {
            return nullptr;
        }
        
        std::unique_lock<std::mutex> lock(mutex_);
        bool gotEngine = cv_.wait_for(lock, remaining, [this] { return !stack_.empty(); });
        
        if (gotEngine && !stack_.empty()) {
            LuaEngine* engine = stack_.top();
            stack_.pop();
            return engine;
        }
        return nullptr;
    }

    /**
     * @brief Initialize pool with pre-created engines
     */
    void initializePool(const std::vector<std::unique_ptr<LuaEngine>>& engines) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = engines.rbegin(); it != engines.rend(); ++it) {
            if (*it) {
                stack_.push(it->get());
            }
        }
    }

    /**
     * @brief Check if pool is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::stack<LuaEngine*> stack_;
    
    // Fast non-blocking pop attempt
    LuaEngine* tryPopFast() {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (lock.owns_lock() && !stack_.empty()) {
            LuaEngine* engine = stack_.top();
            stack_.pop();
            return engine;
        }
        return nullptr;
    }
};

} // namespace fastrules
