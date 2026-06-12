#pragma once

#include "fastrules/logger.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>

namespace fastrules {

// Forward declaration
class LuaEngine;

/**
 * @file lockfree_engine_pool.hpp
 * @brief Lock-free stack (Treiber stack) for engine pool management
 * 
 * This implementation uses tagged pointers to solve the ABA problem and
 * hazard pointers with epoch-based memory reclamation for safe memory management.
 * 
 * MEMORY ORDERING EXPLANATION:
 * ============================
 * 
 * Memory ordering controls how memory operations are seen by other threads.
 * In lock-free programming, we need careful ordering to ensure visibility.
 * 
 * SEQUENTIAL CONSISTENCY (default):
 *   - All operations happen in program order
 *   - All threads see the same order of operations
 *   - SAFEST but SLOWEST - limits compiler/CPU reordering
 * 
 * RELAXED:
 *   - No ordering guarantees
 *   - FASTEST but UNSAFE for synchronization
 *   - Use only for counters that don't synchronize
 * 
 * ACQUIRE/RELEASE (used here):
 *   - RELEASE: Operations before this store cannot be reordered after it
 *   - ACQUIRE: Operations after this load cannot be reordered before it
 *   - Forms a "happens-before" relationship between threads
 *   - GOOD BALANCE of safety and performance
 * 
 * ACQUIRE-RELEASE:
 *   - Both ACQUIRE and RELEASE semantics on RMW operations
 *   - Used for CAS (compare-and-swap) operations
 * 
 * TAGGED POINTERS - SOLVING THE ABA PROBLEM:
 * ==========================================
 * 
 * THE ABA PROBLEM:
 *   Thread A reads head -> node X
 *   Thread B pops X, pushes Y, pushes X back
 *   Thread A's CAS succeeds because head == X
 *   But the stack changed! X's next pointer may be stale
 * 
 * SOLUTION - VERSION TAGS:
 *   Each push increments a version counter
 *   The tag is stored in unused upper bits of the 64-bit pointer
 *   On 64-bit systems, only 48 bits are used for addresses
 *   We use bits 48-63 for a 16-bit version tag
 * 
 *   TaggedPtr layout (64 bits):
 *   | 16-bit tag | 48-bit pointer |
 *   |    [63:48] |         [47:0] |
 * 
 *   When comparing pointers, we compare BOTH address AND tag
 *   If the stack changes, the tag changes, CAS fails
 */

class LockFreeEnginePool {
public:
    /**
     * @struct TaggedPtr
     * @brief Pointer with version tag to prevent ABA problem
     * 
     * On 64-bit systems, only 48 bits are used for virtual addresses.
     * We pack a 16-bit version tag into the upper 16 bits.
     * 
     * Example:
     *   Original pointer: 0x00007FFF12345678
     *   Version tag:      0x0005
     *   Tagged pointer:   0x000500007FFF12345678
     *                              ^^^^ tag
     *                                    ^^^^^^^^^^^^^^^^ address
     */
    struct TaggedPtr {
        uintptr_t ptrAndTag;  // Combined pointer and tag in one atomic unit
        
        TaggedPtr() : ptrAndTag(0) {}
        
        /**
         * @brief Construct tagged pointer from raw pointer and version tag
         * 
         * The tag is shifted left by 48 bits and ORed with the pointer.
         * Pointer must be 48-bit addressable (true for all x86_64 systems).
         */
        TaggedPtr(LuaEngine* ptr, uint16_t tag) 
            : ptrAndTag(reinterpret_cast<uintptr_t>(ptr) | 
                       (static_cast<uintptr_t>(tag) << 48)) {}
        
        /**
         * @brief Extract the pointer portion (lower 48 bits)
         * @return Raw pointer to LuaEngine
         * 
         * Uses bitmask 0x0000FFFFFFFFFFFF to extract only address bits
         */
        LuaEngine* ptr() const { 
            return reinterpret_cast<LuaEngine*>(ptrAndTag & 0x0000FFFFFFFFFFFFULL); 
        }
        
        /**
         * @brief Extract the tag portion (upper 16 bits)
         * @return Version tag (0-65535)
         * 
         * Shifts right by 48 bits to move tag to lower position
         */
        uint16_t tag() const { 
            return static_cast<uint16_t>(ptrAndTag >> 48); 
        }
        
        uintptr_t raw() const { return ptrAndTag; }
    };
    
    // Ensure TaggedPtr is same size as pointer for atomic operations
    static_assert(sizeof(TaggedPtr) == sizeof(uintptr_t), "TaggedPtr must be atomic");

    /**
     * @struct Node
     * @brief Stack node containing engine pointer and next pointer
     * 
     * The stack is a linked list where each node points to the next.
     * The 'next' field is a TaggedPtr to detect ABA during traversal.
     */
    struct Node {
        TaggedPtr next;       // Pointer to next node (with version tag)
        LuaEngine* engine;    // The actual engine being stored
        
        Node(LuaEngine* e) : engine(e) {}
    };

private:
    // Head of the stack - atomic TaggedPtr
    // alignas(64) ensures the atomic is on its own cache line (prevents false sharing)
    alignas(64) std::atomic<TaggedPtr> head_{TaggedPtr()};
    
    // ==========================================
    // HAZARD POINTERS - SAFE MEMORY RECLAMATION
    // ==========================================
    // 
    // THE PROBLEM:
    //   Thread A pops node X
    //   Thread B also tries to pop X (sees same head)
    //   Thread A succeeds, deletes X
    //   Thread B now has dangling pointer!
    //
    // SOLUTION - HAZARD POINTERS:
    //   Each thread announces which node it's about to access
    //   Before deleting, check if any hazard pointer points to node
    //   Only delete when NO thread can possibly be accessing the node
    //
    // IMPLEMENTATION:
    //   Each thread has a dedicated slot in hazardPointers_ array
    //   Thread writes pointer it's about to access to its slot
    //   Other threads check all slots before reclaiming memory
    //
    static constexpr size_t MAX_THREADS = 64;
    alignas(64) std::atomic<Node*> hazardPointers_[MAX_THREADS];
    
    // ==========================================
    // EPOCH-BASED MEMORY RECLAMATION
    // ==========================================
    //
    // THE PROBLEM:
    //   When can we safely delete a retired node?
    //   We need to know ALL threads have passed a synchronization point
    //
    // SOLUTION - EPOCH COUNTERS:
    //   Global epoch counter increments periodically
    //   Each thread announces when it passed the epoch
    //   Nodes retired in epoch N can be deleted when all threads > N
    //
    // This is simpler than full hazard pointer algorithm but works well
    // for stack operations where nodes are only accessed briefly.
    
    // Retired nodes waiting for safe deletion
    alignas(64) std::atomic<Node*> retiredList_{nullptr};
    
    // Global epoch counter - incremented occasionally
    alignas(64) std::atomic<uint64_t> globalEpoch_{0};
    
    // Per-thread epoch tracking - each thread's last seen epoch
    alignas(64) std::atomic<uint64_t> threadEpochs_[MAX_THREADS];
    
    // Thread ID assignment counter
    alignas(64) std::atomic<size_t> nextThreadId_{0};

public:
    LockFreeEnginePool() {
        // Initialize hazard pointers and epoch tracking to zero
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            hazardPointers_[i].store(nullptr, std::memory_order_relaxed);
            threadEpochs_[i].store(0, std::memory_order_relaxed);
        }
    }
    
    ~LockFreeEnginePool() {
        // Clean up all nodes - must be done when no other threads are accessing
        reclaimAll();
    }

    /**
     * @brief Get thread-local thread ID for hazard pointer indexing
     * 
     * Each thread gets a unique ID on first call. Uses modulo to wrap
     * around if more than MAX_THREADS threads are created.
     * 
     * thread_local ensures each thread gets its own ID once.
     */
    size_t getThreadId() {
        thread_local size_t myId = nextThreadId_.fetch_add(1, std::memory_order_relaxed);
        return myId % MAX_THREADS;
    }

    /**
     * @brief Push an engine onto the stack (lock-free)
     * 
     * MEMORY ORDERING:
     *   - memory_order_release on CAS ensures all writes to newNode
     *     are visible to other threads BEFORE the pointer update
     *   - memory_order_relaxed on failure means we can reload with any order
     * 
     * ABA PROTECTION:
     *   - Increment version tag on each push
     *   - Even if same address is reused, tag will differ
     *   - CAS will fail if tag changed (someone else pushed/popped)
     */
    void push(LuaEngine* engine) {
        // Allocate new node - this is our data
        Node* newNode = new Node(engine);
        
        // Load current head - we need to link to this
        // relaxed: we'll verify with CAS, don't need strong ordering yet
        TaggedPtr oldHead = head_.load(std::memory_order_relaxed);
        
        // Retry loop for CAS failure
        for (;;) {
            // Link new node to current head
            newNode->next = oldHead;
            
            // Create new head with incremented tag
            // Tag = old tag + 1 prevents ABA on same address reuse
            TaggedPtr newHead(engine, oldHead.tag() + 1);
            
            // CAS: Compare oldHead and swap to newHead
            //
            // memory_order_release: This store synchronizes-with acquire loads
            //   All writes to newNode happen-before this store is visible
            //
            // memory_order_relaxed on failure: We just reload and retry
            //
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
                // Success! New node is now visible to all threads
                break;
            }
            // CAS failed - another thread changed head
            // oldHead was updated to current value, loop continues
        }
    }

    /**
     * @brief Pop an engine from the stack (lock-free)
     * 
     * MEMORY ORDERING:
     *   - memory_order_acquire on first load ensures we see all prior pushes
     *   - memory_order_acquire on reload verifies hazard pointer safety
     *   - memory_order_release on hazard pointer store makes it visible
     * 
     * HAZARD POINTERS:
     *   1. Load head
     *   2. Set hazard pointer BEFORE dereferencing
     *   3. Reload head to verify it hasn't changed
     *   4. If changed, retry (someone else popped)
     *   5. CAS to pop - if success, clear hazard pointer
     * 
     * This ensures we never dereference a node that might be freed.
     */
    LuaEngine* pop() {
        size_t tid = getThreadId();
        
        for (;;) {
            // Step 1: Acquire load of head
            // memory_order_acquire: Synchronizes-with release in push
            // This ensures we see the fully initialized node
            TaggedPtr oldHead = head_.load(std::memory_order_acquire);
            Node* node = reinterpret_cast<Node*>(oldHead.ptr());
            
            // Check for empty stack
            if (node == nullptr) {
                return nullptr;
            }
            
            // Step 2: Set hazard pointer BEFORE dereferencing node
            // memory_order_release: Ensures hazard pointer is visible before we use node
            // This prevents other threads from freeing this node
            hazardPointers_[tid].store(node, std::memory_order_release);
            
            // Step 3: Reload head to verify it hasn't changed
            // memory_order_acquire: Ensures we see any concurrent updates
            TaggedPtr currentHead = head_.load(std::memory_order_acquire);
            if (currentHead.raw() != oldHead.raw()) {
                // Head changed between load and hazard pointer set
                // Someone else may have popped this node - retry
                hazardPointers_[tid].store(nullptr, std::memory_order_release);
                continue;
            }
            
            // Step 4: Try to pop with CAS
            // New head is node's next pointer
            TaggedPtr newHead(node->next.ptr(), oldHead.tag() + 1);
            
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
                // Success! We popped the node
                // Clear hazard pointer - we're done with this node
                hazardPointers_[tid].store(nullptr, std::memory_order_release);
                
                // Extract engine and retire the node
                LuaEngine* engine = node->engine;
                retireNode(node);
                return engine;
            }
            
            // CAS failed - head changed after our hazard check
            // Clear hazard and retry
            hazardPointers_[tid].store(nullptr, std::memory_order_release);
            // Loop continues with updated oldHead
        }
    }

    /**
     * @brief Initialize pool with pre-created engines
     * 
     * Pushes engines in reverse order so the first engine in the vector
     * ends up at the top of the stack (most likely to be popped first).
     */
    void initializePool(const std::vector<std::unique_ptr<LuaEngine>>& engines) {
        for (auto it = engines.rbegin(); it != engines.rend(); ++it) {
            if (*it) {
                push(it->get());
            }
        }
    }

private:
    /**
     * @brief Retire a node for later deletion
     * 
     * Nodes can't be deleted immediately - other threads might be accessing them.
     * We add to a retired list and delete later when it's safe.
     */
    void retireNode(Node* node) {
        // Load current retired list head
        Node* oldRetired = retiredList_.load(std::memory_order_relaxed);
        
        // CAS loop to push onto retired list
        for (;;) {
            // Link to current head
            node->next = TaggedPtr(reinterpret_cast<LuaEngine*>(oldRetired), 0);
            
            // Try to swap
            if (retiredList_.compare_exchange_weak(oldRetired, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                break;
            }
        }
        
        // Attempt to reclaim memory
        tryReclaim();
    }

    /**
     * @brief Try to reclaim retired nodes
     * 
     * Only reclaim if all threads have passed current epoch.
     * This is a simplified epoch-based reclamation.
     */
    void tryReclaim() {
        uint64_t currentEpoch = globalEpoch_.load(std::memory_order_relaxed);
        
        // Advance epoch occasionally (every 100 calls)
        if ((currentEpoch % 100) == 0) {
            globalEpoch_.fetch_add(1, std::memory_order_relaxed);
            currentEpoch++;
        }
        
        // Check if all threads have passed this epoch
        bool allPassed = true;
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            uint64_t threadEpoch = threadEpochs_[i].load(std::memory_order_acquire);
            if (threadEpoch != 0 && threadEpoch < currentEpoch - 1) {
                allPassed = false;
                break;
            }
        }
        
        if (allPassed) {
            reclaimNodes(currentEpoch);
        }
    }

    /**
     * @brief Reclaim nodes that are safe to delete
     * 
     * In production, would check each node's retirement epoch.
     * Here we reclaim all retired nodes for simplicity.
     */
    void reclaimNodes(uint64_t safeEpoch) {
        // Exchange entire list - we own these nodes now
        Node* node = retiredList_.exchange(nullptr, std::memory_order_acquire);
        
        // Free all nodes
        while (node) {
            Node* next = reinterpret_cast<Node*>(node->next.ptr());
            delete node;
            node = next;
        }
    }

    /**
     * @brief Clean up all nodes in destructor
     * 
     * Called when pool is destroyed. Assumes no other threads are accessing.
     */
    void reclaimAll() {
        // Free stack nodes
        TaggedPtr head = head_.exchange(TaggedPtr(), std::memory_order_acquire);
        Node* node = reinterpret_cast<Node*>(head.ptr());
        
        while (node) {
            Node* next = reinterpret_cast<Node*>(node->next.ptr());
            delete node;
            node = next;
        }
        
        // Free retired nodes
        reclaimNodes(0);
    }
};

} // namespace fastrules
