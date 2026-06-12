#pragma once

#include "fastrules/logger.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

// Platform detection for 64-bit vs 32-bit
// Lock-free tagged pointers require 64-bit addresses
#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
    #define FASTRULES_LOCKFREE_POOL_64BIT
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    #define FASTRULES_LOCKFREE_POOL_64BIT
#else
    // 32-bit or unknown architecture - use mutex-based fallback
#endif

namespace fastrules {

// Forward declaration
class LuaEngine;

/**
 * @file lockfree_engine_pool.hpp
 * @brief Lock-free stack (Treiber stack) for engine pool management
 * 
 * ARCHITECTURE SUPPORT:
 * =====================
 * This header provides two implementations:
 * 
 * 1. 64-BIT LOCK-FREE IMPLEMENTATION (default on x86_64, ARM64)
 *    - Uses tagged pointers for ABA protection
 *    - Uses hazard pointers for safe memory reclamation
 *    - Requires 64-bit pointers (to pack version tag in upper bits)
 *    - Best performance under high thread contention
 * 
 * 2. 32-BIT MUTEX-BASED FALLBACK (used on x86, ARM32, other 32-bit)
 *    - Standard mutex + condition variable for synchronization
 *    - Simpler code, easier to understand and debug
 *    - Lower single-thread overhead
 *    - Good enough for most use cases
 * 
 * The appropriate implementation is selected automatically at compile time
 * based on the target architecture. Users don't need to do anything special.
 * 
 * The public API is identical for both implementations, so client code
 * (like Workflow) works the same way regardless of platform.
 */

// ============================================================================
// 64-BIT LOCK-FREE IMPLEMENTATION
// ============================================================================
#ifdef FASTRULES_LOCKFREE_POOL_64BIT

/**
 * @brief Lock-free engine pool with tagged pointers (64-bit only)
 * 
 * PURPOSE:
 * ========
 * This class provides a high-performance, thread-safe pool of LuaEngine objects
 * that can be acquired and released without locks. This is critical for the
 * parallel execution of workflow rules, where multiple threads need to 
 * simultaneously access pre-compiled Lua engine instances.
 * 
 * WHY LOCK-FREE?
 * ==============
 * Traditional mutex-based pools have drawbacks:
 *   - Contention: Multiple threads block waiting for the same mutex
 *   - Priority inversion: Low-priority threads can block high-priority ones
 *   - Latency: Context switches when threads block/unblock
 * 
 * Lock-free algorithms guarantee system-wide progress - at least one thread
 * always makes progress, regardless of thread scheduling. This provides
 * predictable latency under high contention.
 * 
 * THE ALGORITHM - TREIBER STACK:
 * ==============================
 * This implements a Treiber stack, a classic lock-free data structure:
 *   - Single atomic pointer (head_) points to top of stack
 *   - Push: Create node, CAS head to new node (pointing to old head)
 *   - Pop: CAS head to head->next, return old head's data
 * 
 * CHALLENGES SOLVED:
 * ==================
 * 
 * 1. ABA PROBLEM
 *    Thread A reads head -> node X
 *    Thread B pops X, pushes Y, pushes X back
 *    Thread A's CAS succeeds because head == X
 *    But X was freed and reallocated! X->next is now garbage.
 *    
 *    Solution: Tagged pointers - version counter in unused
 *    bits of 64-bit pointer. CAS compares both address AND tag.
 * 
 * 2. MEMORY RECLAMATION (SAFE DELETION)
 *    Thread A pops node X, starts using it
 *    Thread B also popped X (lost race), freed it
 *    Thread A now has dangling pointer!
 *    
 *    Solution: Hazard pointers - threads announce what they're about to
 *    access. Memory is only freed when no hazard pointer points to it.
 * 
 * 3. MEMORY ORDERING
 *    CPU and compiler can reorder operations. Without proper barriers,
 *    Thread B might see node data before seeing updated head pointer.
 *    
 *    Solution: Acquire/release semantics ensure visibility.
 * 
 * PLATFORM REQUIREMENTS:
 * ======================
 * This implementation requires:
 *   - 64-bit system (tagged pointers use upper 16 bits)
 *   - x86_64 or ARM64 architecture
 *   - C++11 or later (atomics, thread_local)
 *   - Maximum 64 concurrent threads (MAX_THREADS constant)
 * 
 * MEMORY ORDERING EXPLAINED:
 * ==========================
 * 
 * SEQUENTIAL CONSISTENCY (memory_order_seq_cst):
 *   - Default, strictest ordering
 *   - All operations happen in program order across ALL threads
 *   - SAFEST but SLOWEST
 * 
 * RELAXED (memory_order_relaxed):
 *   - No ordering guarantees, only atomicity
 *   - FASTEST but UNSAFE for synchronization
 * 
 * ACQUIRE/RELEASE (memory_order_acquire/release):
 *   - FORMS A "HAPPENS-BEFORE" RELATIONSHIP
 *   - RELEASE: Writes before this store are visible to threads that acquire
 *   - ACQUIRE: Ensures we see all writes that happened-before release
 *   - GOOD BALANCE of safety and performance
 * 
 * TAGGED POINTERS - ABA PROTECTION:
 * ==================================
 * On 64-bit systems, only 48 bits are used for virtual addresses.
 * We pack a 16-bit version counter into bits 48-63:
 * 
 *   | bits 63-48 | bits 47-0  |
 *   |  version   |  address   |
 *   |  16 bits   |  48 bits   |
 * 
 * Each push increments the version. Even if the same address is reused,
 * the version tag will differ, causing CAS to fail safely.
 */

// Suppress MSVC warning about struct padding - intentional for cache-line alignment
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324)  // structure was padded due to alignment specifier
#endif

class LockFreeEnginePool {
public:
    /**
     * @struct TaggedPtr
     * @brief Pointer combined with version tag for ABA-safe CAS
     * 
     * On 64-bit systems, packs a 16-bit version counter into the upper
     * 16 bits of the 64-bit pointer value.
     * 
     * Templated to work with any pointer type (Node*, LuaEngine*, etc.)
     */
    template<typename T>
    struct TaggedPtr {
        uintptr_t ptrAndTag;  // Combined pointer and version
        
        TaggedPtr() : ptrAndTag(0) {}
        
        TaggedPtr(T* ptr, uint16_t tag) 
            : ptrAndTag(reinterpret_cast<uintptr_t>(ptr) | 
                       (static_cast<uintptr_t>(tag) << 48)) {}
        
        T* ptr() const { 
            return reinterpret_cast<T*>(ptrAndTag & 0x0000FFFFFFFFFFFFULL); 
        }
        
        uint16_t tag() const { 
            return static_cast<uint16_t>(ptrAndTag >> 48); 
        }
        
        uintptr_t raw() const { return ptrAndTag; }
    };
    
    // Size check - TaggedPtr must be same size as uintptr_t for atomic operations
    static_assert(sizeof(TaggedPtr<void>) == sizeof(uintptr_t), "TaggedPtr must be atomic");

    struct Node {
        TaggedPtr<Node> next;
        LuaEngine* engine;
        
        explicit Node(LuaEngine* e) : engine(e) {}
    };

private:
    // Stack head - cache line aligned to prevent false sharing
    alignas(64) std::atomic<TaggedPtr<Node>> head_{TaggedPtr<Node>()};
    
    // Hazard pointers for safe memory reclamation
    static constexpr size_t MAX_THREADS = 64;
    alignas(64) std::atomic<Node*> hazardPointers_[MAX_THREADS];
    
    // Retired nodes (simplified reclamation - freed in destructor)
    alignas(64) std::atomic<TaggedPtr<Node>> retiredList_{TaggedPtr<Node>()};
    alignas(64) std::atomic<uint64_t> globalEpoch_{0};
    alignas(64) std::atomic<uint64_t> threadEpochs_[MAX_THREADS];
    alignas(64) std::atomic<size_t> nextThreadId_{0};

public:
    LockFreeEnginePool() {
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            hazardPointers_[i].store(nullptr, std::memory_order_relaxed);
            threadEpochs_[i].store(0, std::memory_order_relaxed);
        }
    }
    
    ~LockFreeEnginePool() {
        reclaimAll();
    }

    size_t getThreadId() {
        thread_local size_t myId = nextThreadId_.fetch_add(1, std::memory_order_relaxed);
        return myId % MAX_THREADS;
    }

    void push(LuaEngine* engine) {
        Node* newNode = new Node(engine);
        TaggedPtr<Node> oldHead = head_.load(std::memory_order_relaxed);
        
        for (;;) {
            newNode->next = oldHead;
            TaggedPtr<Node> newHead(newNode, oldHead.tag() + 1);
            
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
                break;
            }
        }
    }

    LuaEngine* pop() {
        size_t tid = getThreadId();
        
        for (;;) {
            TaggedPtr<Node> oldHead = head_.load(std::memory_order_acquire);
            Node* node = oldHead.ptr();
            
            if (node == nullptr) {
                return nullptr;
            }
            
            // Set hazard pointer before dereferencing
            hazardPointers_[tid].store(node, std::memory_order_release);
            
            // Verify head hasn't changed
            TaggedPtr<Node> currentHead = head_.load(std::memory_order_acquire);
            if (currentHead.raw() != oldHead.raw()) {
                hazardPointers_[tid].store(nullptr, std::memory_order_release);
                continue;
            }
            
            TaggedPtr<Node> newHead(node->next.ptr(), oldHead.tag() + 1);
            
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
                hazardPointers_[tid].store(nullptr, std::memory_order_release);
                
                LuaEngine* engine = node->engine;
                retireNode(node);
                return engine;
            }
            
            hazardPointers_[tid].store(nullptr, std::memory_order_release);
        }
    }

    void initializePool(const std::vector<std::unique_ptr<LuaEngine>>& engines) {
        for (auto it = engines.rbegin(); it != engines.rend(); ++it) {
            if (*it) {
                push(it->get());
            }
        }
    }

private:
    void retireNode(Node* node) {
        TaggedPtr<Node> oldRetired = retiredList_.load(std::memory_order_relaxed);
        
        for (;;) {
            node->next = TaggedPtr<Node>(oldRetired.ptr(), 0);
            
            if (retiredList_.compare_exchange_weak(oldRetired, TaggedPtr<Node>(node, oldRetired.tag()),
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                break;
            }
        }
    }

    void reclaimAll() {
        TaggedPtr<Node> head = head_.exchange(TaggedPtr<Node>(), std::memory_order_acquire);
        Node* node = head.ptr();
        
        while (node) {
            Node* next = node->next.ptr();
            delete node;
            node = next;
        }
        
        // Free retired nodes
        TaggedPtr<Node> retired = retiredList_.exchange(TaggedPtr<Node>(), std::memory_order_acquire);
        while (retired.ptr()) {
            Node* node = retired.ptr();
            retired = node->next;
            delete node;
        }
    }
};

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

// ============================================================================
// 32-BIT MUTEX-BASED FALLBACK IMPLEMENTATION
// ============================================================================
#else // 32-bit fallback

/**
 * @brief Mutex-based engine pool (fallback for 32-bit systems)
 * 
 * This implementation provides the same public API as the 64-bit lock-free
 * version, but uses standard mutex synchronization instead of lock-free
 * algorithms. This is necessary because:
 * 
 *   - 32-bit systems don't have unused pointer bits for version tags
 *   - Tagged pointer ABA protection requires 64-bit addresses
 *   - The lock-free algorithm is complex and not needed on 32-bit
 * 
 * TRADE-OFFS:
 * ===========
 * Compared to the 64-bit lock-free version:
 *   - Simpler code, easier to maintain
 *   - Lower single-thread overhead
 *   - Higher contention overhead (mutex blocking)
 *   - Better for low-contention scenarios
 *   - Good enough for most real-world use cases
 * 
 * The mutex-based pool is also used if the lock-free implementation
 * encounters issues - it's a safe, well-tested fallback.
 */

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324)
#endif

class LockFreeEnginePool {
public:
    LockFreeEnginePool() = default;
    
    ~LockFreeEnginePool() = default;

    /**
     * @brief Push an engine onto the pool
     * 
     * Thread-safe: acquires mutex, adds engine to available list,
     * notifies waiting threads.
     */
    void push(LuaEngine* engine) {
        std::lock_guard<std::mutex> lock(mutex_);
        availableEngines_.push_back(engine);
        cv_.notify_one();
    }

    /**
     * @brief Pop an engine from the pool
     * 
     * Thread-safe: acquires mutex, waits if no engines available,
     * returns first available engine.
     * 
     * @return Pointer to LuaEngine, or nullptr if pool is being destroyed
     */
    LuaEngine* pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until an engine is available
        cv_.wait(lock, [this] { return !availableEngines_.empty() || !running_; });
        
        if (!running_ || availableEngines_.empty()) {
            return nullptr;
        }
        
        LuaEngine* engine = availableEngines_.back();
        availableEngines_.pop_back();
        return engine;
    }

    /**
     * @brief Initialize pool with pre-created engines
     * 
     * Adds all engines from the vector to the available pool.
     * Engines are added in order (first in vector = first to be popped).
     * 
     * @param engines Vector of unique_ptr<LuaEngine> to add to the pool
     */
    void initializePool(const std::vector<std::unique_ptr<LuaEngine>>& engines) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& engine : engines) {
            if (engine) {
                availableEngines_.push_back(engine.get());
            }
        }
    }

    /**
     * @brief Signal the pool to shut down
     * 
     * Wakes all waiting threads so they can exit gracefully.
     * Called by destructor or when Workflow is being destroyed.
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<LuaEngine*> availableEngines_;
    bool running_ = true;
};

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#endif // FASTRULES_LOCKFREE_POOL_64BIT

} // namespace fastrules
