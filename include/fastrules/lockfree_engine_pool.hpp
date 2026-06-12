#pragma once

#include "fastrules/logger.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stack>

namespace fastrules {

// Forward declaration
class LuaEngine;

/**
 * @brief Thread-safe engine pool using mutex-based stack
 * 
 * This implementation uses a std::stack with mutex protection.
 * It provides reliable thread-safety without the complexity and
 * potential livelock issues of lock-free algorithms.
 */
class LockFreeEnginePool {
public:
    LockFreeEnginePool() = default;
    
    ~LockFreeEnginePool() = default;

    /**
     * @brief Push an engine onto the pool
     * 
     * Thread-safe: acquires mutex, adds engine to stack.
     */
    void push(LuaEngine* engine) {
        std::lock_guard<std::mutex> lock(mutex_);
        stack_.push(engine);
    }

    /**
     * @brief Pop an engine from the pool
     * 
     * Thread-safe: acquires mutex, returns top engine if available.
     * Returns nullptr if pool is empty.
     * 
     * @return Pointer to LuaEngine, or nullptr if pool is empty
     */
    LuaEngine* pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stack_.empty()) {
            return nullptr;
        }
        LuaEngine* engine = stack_.top();
        stack_.pop();
        return engine;
    }

    /**
     * @brief Initialize pool with pre-created engines
     * 
     * Adds all engines from the vector to the pool.
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

    /**
     * @brief Get approximate size (for debugging)
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.size();
    }

private:
    mutable std::mutex mutex_;
    std::stack<LuaEngine*> stack_;
};

} // namespace fastrules
