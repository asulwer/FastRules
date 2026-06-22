/**
 * @file engine_pool.hpp
 * @brief Thread-safe pool of LuaEngines for parallel execution
 * 
 * EnginePool provides a lock-free (or low-contention) way to manage
 * LuaEngine instances for parallel rule execution. Lua states are NOT
 * thread-safe, so each thread needs its own engine.
 * 
 * Design:
 * - Lock-free stack using atomic operations
 * - Spin-wait with _mm_pause() for efficiency
 * - Timeout support for tryPop()
 * 
 * Thread Safety:
 * - push/pop: Thread-safe (lock-free)
 * - tryPop: Thread-safe
 * 
 * Usage Pattern:
 * @code
 * EnginePool pool;
 * 
 * // Populate with engines
 * for (auto& engine : engines) {
 *     pool.push(engine.get());
 * }
 * 
 * // Worker thread:
 * LuaEngine* engine = pool.pop();  // Non-blocking: nullptr if pool is empty
 * if (engine) {
 *     // ... use engine ...
 *     pool.push(engine);  // Return to pool
 * }
 * @endcode
 * 
 * Performance:
 * - push: O(1) atomic
 * - pop: O(1) atomic, may spin
 * - tryPop: O(1) atomic, non-blocking
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>

namespace fastrules {

// Forward declaration
class LuaEngine;

/**
 * @brief Node in the lock-free stack
 * 
 * Simple linked list node storing an engine pointer and next pointer.
 */
struct EngineNode {
    LuaEngine* engine;       ///< The engine instance
    std::shared_ptr<EngineNode> next;        ///< Next node in stack
    
    /// @brief Construct from engine pointer
    explicit EngineNode(LuaEngine* e) : engine(e) {}
};

/**
 * @brief Lock-free pool of LuaEngines
 * 
 * Implements a Treiber stack (lock-free stack) for managing engines.
 * This is thread-safe and provides wait-free push and pop operations.
 * 
 * Memory Management:
 * The pool doesn't own the engines - they must be kept alive externally.
 * The pool only stores pointers.
 * 
 * Example:
 * @code
 * // Create engines
 * std::vector<std::unique_ptr<LuaEngine>> engines;
 * for (int i = 0; i < 4; ++i) {
 *     engines.push_back(std::make_unique<LuaEngine>();
 * }
 * 
 * // Create pool
 * EnginePool pool;
 * for (auto& e : engines) {
 *     pool.push(e.get());
 * }
 * 
 * // Use pool from multiple threads
 * std::thread worker([&pool]() {
 *     LuaEngine* engine = pool.pop();
 *     // ... use engine ...
 *     pool.push(engine);
 * });
 * @endcode
 */
class EnginePool {
public:
    /**
     * @brief Construct an empty pool
     */
    EnginePool() = default;

    /**
     * @brief Destructor
     * 
     * Note: Does NOT delete engines - caller owns them.
     */
    ~EnginePool() = default;

    /// @brief Disable copy
    EnginePool(const EnginePool&) = delete;
    
    /// @brief Disable copy assignment
    EnginePool& operator=(const EnginePool&) = delete;
    
    /// @brief Disable move (atomic operations)
    EnginePool(EnginePool&&) = delete;
    
    /// @brief Disable move assignment
    EnginePool& operator=(EnginePool&&) = delete;

    /**
     * @brief Push an engine onto the pool
     * 
     * Lock-free operation using compare-and-swap.
     * 
     * @param engine The engine to add
     */
    void push(LuaEngine* engine) {
        auto node = std::make_shared<EngineNode>(engine);
        auto current = head_.load(std::memory_order_relaxed);
        
        do {
            node->next = current;
        } while (!head_.compare_exchange_weak(
            current, node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    /**
     * @brief Pop an engine from the pool
     * 
     * Returns an engine if available, or nullptr if pool is empty.
     * Non-blocking - returns immediately.
     * 
     * @return Pointer to an engine, or nullptr if pool is empty
     */
    LuaEngine* pop() {
        auto current = head_.load(std::memory_order_relaxed);
        
        while (current != nullptr) {
            // Try to CAS head to next node
            if (head_.compare_exchange_weak(
                current, current->next,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
                LuaEngine* engine = current->engine;
                // current shared_ptr will be destroyed when it goes out of scope
                return engine;
            }
            
            // CAS failed, reload head
            current = head_.load(std::memory_order_relaxed);
            
            // Spin-wait with pause
            #if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
                _mm_pause();
            #elif defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
            #endif
        }
        
        // Pool is empty - return nullptr
        return nullptr;
    }

    /**
     * @brief Try to pop an engine with timeout
     *
     * Waits up to @p timeout for an engine to become available, spinning while
     * the pool is empty so that an engine returned by another thread can be
     * acquired before the deadline. Returns nullptr only once the deadline
     * passes without an engine being available.
     *
     * @param timeout Maximum time to wait
     * @return Pointer to engine, or nullptr on timeout
     */
    LuaEngine* tryPop(std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        for (;;) {
            auto current = head_.load(std::memory_order_relaxed);

            if (current != nullptr) {
                if (head_.compare_exchange_weak(
                    current, current->next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                    return current->engine;
                }
                // CAS lost a race; retry immediately without checking the clock.
            } else if (std::chrono::steady_clock::now() >= deadline) {
                return nullptr;  // Empty and out of time
            }

            // Brief pause to reduce contention / busy-spinning.
            #if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
                _mm_pause();
            #elif defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
            #endif
        }
    }

    /**
     * @brief Check if pool is empty
     * 
     * @return true if pool has no engines
     */
    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_relaxed) == nullptr;
    }

private:
    std::atomic<std::shared_ptr<EngineNode>> head_{nullptr};  ///< Atomic head of stack
};

} // namespace fastrules
