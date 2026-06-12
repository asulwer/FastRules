#pragma once

#include "fastrules/logger.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <stack>

namespace fastrules {

// Forward declaration
class LuaEngine;

/**
 * @brief Thread-safe engine pool with try-lock optimization
 * 
 * Fast path: Try to acquire mutex without blocking
 * Slow path: Block with condition variable
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
     * @brief Try to pop with timeout
     * 
     * Blocks until engine available or timeout.
     * Used by acquireEngine() in workflow.cpp
     */
    LuaEngine* tryPop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait with timeout
        bool gotEngine = cv_.wait_for(lock, timeout, [this] { return !stack_.empty(); });
        
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
};

} // namespace fastrules
