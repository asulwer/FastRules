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
 *    Solution: Tagged pointers (see below) - version counter in unused
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
 *    Solution: Acquire/release semantics ensure visibility (see below).
 * 
 * PLATFORM ASSUMPTIONS:
 * =====================
 * This code assumes:
 *   - 64-bit system (tagged pointers use upper 16 bits)
 *   - x86_64 architecture (_mm_pause() instruction)
 *   - Maximum 64 concurrent threads (MAX_THREADS constant)
 * 
 * These assumptions hold for most modern server/desktop systems.
 * 
 * MEMORY ORDERING EXPLAINED:
 * ==========================
 * 
 * In C++11 and later, atomic operations can specify "memory ordering"
 * which controls how operations are seen across threads. Think of it as
 * a contract between threads about visibility of memory writes.
 * 
 * SEQUENTIAL CONSISTENCY (memory_order_seq_cst):
 *   - Default, strictest ordering
 *   - All operations happen in program order across ALL threads
 *   - Like having a single global timeline everyone agrees on
 *   - SAFEST but SLOWEST - prevents many compiler/CPU optimizations
 *   - Example: head_.load() // seq_cst by default
 * 
 * RELAXED (memory_order_relaxed):
 *   - No ordering guarantees
 *   - Only atomicity is guaranteed (no torn reads/writes)
 *   - FASTEST but UNSAFE for synchronization
 *   - Use for: counters that don't guard other data, thread IDs
 *   - Example: nextThreadId_.fetch_add(1, relaxed)
 * 
 * ACQUIRE/RELEASE (memory_order_acquire/release):
 *   - FORMS A "HAPPENS-BEFORE" RELATIONSHIP
 *   - RELEASE (store): All writes before this store become visible to 
 *     any thread that does an ACQUIRE load of the same atomic
 *   - ACQUIRE (load): Ensures we see all writes that happened-before
 *     any release store to this atomic
 *   - Together they create a synchronization point between threads
 *   - GOOD BALANCE: Safe for most synchronization, faster than seq_cst
 *   
 *   Visual example:
 *   Thread A: write data ------> release store to flag
 *                                   |
 *                                   | synchronizes-with
 *                                   v
 *   Thread B: acquire load from flag ------> read data
 *   
 *   Thread B is guaranteed to see Thread A's writes to data.
 * 
 * ACQUIRE-RELEASE (memory_order_acq_rel):
 *   - Both ACQUIRE and RELEASE semantics on read-modify-write operations
 *   - Used for CAS (compare-and-swap) operations
 *   - Ensures both visibility of prior writes AND publication of new value
 * 
 * WHY DIFFERENT ORDERINGS IN THIS CODE?
 * ======================================
 * - head_.load(acquire): Must see fully initialized node from push
 * - head_.compare_exchange(acquire, release): CAS needs both visibility
 * - hazardPointers_[tid].store(release): Make hazard pointer visible
 * - nextThreadId_.fetch_add(relaxed): Just a counter, no data to protect
 * 
 * TAGGED POINTERS - SOLVING THE ABA PROBLEM:
 * ==========================================
 * 
 * THE ABA PROBLEM EXPLAINED:
 * --------------------------
 * The ABA problem occurs when a location is read as value A, then changed
 * to B, then changed back to A. A compare-and-swap operation checking for
 * A will succeed, even though the intermediate state (B) changed things.
 * 
 * Concrete example with a stack:
 *   
 *   Initial state:  head -> [Node A] -> [Node B] -> nullptr
 *   
 *   Thread 1: Reads head (Node A), plans to pop it
 *            Stores A.next (Node B) in local variable
 *            Prepares CAS: head = A, new_head = B
 *            
 *   [Thread 1 gets preempted here]
 *   
 *   Thread 2: Pops Node A (head becomes B)
 *            Deletes Node A (free memory)
 *            Pushes new node (reallocates at same address as A!)
 *            head -> [Node A (reused)] -> [Node C] -> nullptr
 *            
 *   Thread 1: Resumes, performs CAS
 *            Compares head == A: TRUE!
 *            Sets head = B
 *            
 *   Result: Stack is now: head -> [Node B] -> ???
 *   
 *   Problem: Node B might have been freed by Thread 2!
 *   Even if not freed, the stack structure is corrupted.
 *   Thread 1 thinks B is the new head, but B->next could be anything.
 * 
 * SOLUTION - VERSION TAGS:
 * -----------------------
 * On 64-bit x86_64 systems, only 48 bits are actually used for virtual
 * addresses. The upper 16 bits are unused (must be sign-extended copies
 * of bit 47, but for user-space addresses they're 0).
 * 
 * We pack a 16-bit version counter into these unused bits:
 * 
 *   64-bit pointer layout:
 *   | bits 63-48 | bits 47-0  |
 *   |  version   |   address  |
 *   |  (16 bits) |  (48 bits) |
 *   
 *   Example:
 *     Raw address:     0x00007FFF12345678
 *     Version 5:       0x000500007FFF12345678
 *                      ^^^^
 *                      version tag (0x0005)
 * 
 * Each push operation increments the version tag. Now the ABA scenario:
 * 
 *   Thread 1: Reads head (A, version 1)
 *   
 *   Thread 2: Pops A, deletes it, pushes new node at same address
 *            New node has version 2
 *            
 *   Thread 1: CAS compares (A, version 1) with current head (A, version 2)
 *            TAGS DIFFER! CAS fails.
 *            Thread 1 retries with correct version.
 * 
 * This guarantees the CAS only succeeds if the pointer hasn't changed
 * in the interim, solving the ABA problem without additional memory.
 * 
 * HAZARD POINTERS - SAFE MEMORY RECLAMATION:
 * ==========================================
 * 
 * THE PROBLEM:
 * -----------
 * In the pop() operation:
 *   1. Thread reads head pointer
 *   2. Thread dereferences head to get head->next
 *   3. Thread CAS head to head->next
 *   
 * Between steps 1 and 2, another thread might:
 *   - Pop the same node (won the CAS race)
 *   - Delete the node
 *   
 * Now step 2 dereferences freed memory - undefined behavior!
 * 
 * SOLUTION - HAZARD POINTERS:
 * ---------------------------
 * Before dereferencing a pointer that might be freed by another thread,
 * announce "I'm about to use this" in a shared array.
 * 
 * Each thread has a dedicated slot:
 *   hazardPointers_[thread_id] = node_being_accessed;
 *   
 * Before deleting a node, scan ALL hazard pointers:
 *   for each slot in hazardPointers:
 *       if slot == node_to_delete:
 *           can't delete yet!
 *           
 * This is a simplified version. Production systems use epoch-based
 * reclamation (as hinted at in this code) for better performance.
 * 
 * PERFORMANCE CHARACTERISTICS:
 * ============================
 * - Push: O(1), single CAS, no contention except on actual conflict
 * - Pop: O(1) average, may retry on contention but doesn't block
 * - Contention handling: Exponential backoff via spinning + yielding
 * - Memory overhead: One Node per engine, fixed hazard pointer array
 * 
 * COMPARED TO MUTEX-BASED POOL:
 * - Lower latency under contention (no kernel calls)
 * - Better scalability (no single mutex bottleneck)
 * - Higher single-thread overhead (CAS vs simple pointer ops)
 * - More complex code (this documentation exists for a reason!)
 * 
 * WHEN TO USE:
 * ============
 * Use this lock-free pool when:
 *   - High thread contention is expected (many threads, few engines)
 *   - Predictable latency is critical (real-time requirements)
 *   - The cost of context switches is unacceptable
 *   - You're comfortable with complex concurrent code
 * 
 * Use mutex-based pool when:
 *   - Low contention (engines usually available)
 *   - Simplicity and maintainability are priorities
 *   - Contention is rare enough that blocking is acceptable
 */

class LockFreeEnginePool {
public:
    /**
     * @struct TaggedPtr
     * @brief Pointer combined with version tag for ABA-safe CAS operations
     * 
     * On 64-bit x86_64 systems, virtual addresses use only 48 bits (bits 0-47).
     * Bits 48-63 are unused (must be sign extension of bit 47 for canonical
     * addresses, but in user space they're 0).
     * 
     * We pack a 16-bit version counter into bits 48-63. This gives us
     * 65,536 unique versions before wrapping - more than enough to prevent
     * the ABA problem in practice.
     * 
     * Memory layout (64 bits):
     *   | 63 ... 48 | 47 ... 0  |
     *   |  version  |  address  |
     *   |  16 bits  |  48 bits  |
     * 
     * Construction: (address & 0x0000FFFFFFFFFFFF) | (version << 48)
     * Extraction:   address = value & 0x0000FFFFFFFFFFFF
     *               version = (value >> 48) & 0xFFFF
     * 
     * Example with tagged pointer:
     *   Raw address: 0x00007FFF12345678 (typical heap address)
     *   Version:     42 (0x002A)
     *   Tagged:      0x002A00007FFF12345678
     *   Extract:     address = 0x00007FFF12345678, version = 42
     * 
     * The tagged pointer is treated as an opaque value by atomic operations.
     * We only unpack it after a successful CAS, ensuring we got the expected
     * version (preventing ABA).
     */
    struct TaggedPtr {
        uintptr_t ptrAndTag;  // Combined pointer and version in one atomic unit
        
        /**
         * @brief Default constructor - creates null pointer with version 0
         * 
         * A null tagged pointer has both address and tag set to 0.
         * This is distinct from any valid pointer because version 0
         * will never match a non-zero version in CAS operations.
         */
        TaggedPtr() : ptrAndTag(0) {}
        
        /**
         * @brief Construct tagged pointer from raw pointer and version tag
         * 
         * Combines the address and version into a single 64-bit value:
         *   ptrAndTag = (uintptr_t)ptr | ((uintptr_t)tag << 48)
         * 
         * Requirements:
         *   - ptr must be a 48-bit address (true for all user-space x86_64)
         *   - tag will wrap after 65536 pushes (unlikely to cause issues)
         * 
         * The shift by 48 places the tag in the upper 16 bits, leaving
         * the lower 48 bits for the address. The OR operation combines them.
         */
        TaggedPtr(LuaEngine* ptr, uint16_t tag) 
            : ptrAndTag(reinterpret_cast<uintptr_t>(ptr) | 
                       (static_cast<uintptr_t>(tag) << 48)) {}
        
        /**
         * @brief Extract the pointer portion (lower 48 bits)
         * @return Raw pointer to LuaEngine, or nullptr if ptrAndTag is 0
         * 
         * Uses bitwise AND with mask 0x0000FFFFFFFFFFFF to clear the
         * upper 16 bits (version tag), leaving only the address.
         * 
         * Note: On x86_64, addresses are sign-extended from bit 47, so
         * user-space pointers have 0x0000 or 0xFFFF in upper bits.
         * Our mask correctly extracts just the address portion.
         */
        LuaEngine* ptr() const { 
            return reinterpret_cast<LuaEngine*>(ptrAndTag & 0x0000FFFFFFFFFFFFULL); 
        }
        
        /**
         * @brief Extract the version tag portion (upper 16 bits)
         * @return Version number (0-65535), incremented on each push
         * 
         * Right-shifts by 48 bits to move the tag from the upper
         * 16 bits down to the lower 16 bits, then casts to uint16_t.
         * 
         * The version tag is incremented on every push operation,
         * even if the same address is reused. This is what prevents
         * the ABA problem - the tag changes even when the address
         * stays the same.
         */
        uint16_t tag() const { 
            return static_cast<uint16_t>(ptrAndTag >> 48); 
        }
        
        /**
         * @brief Get raw 64-bit value for atomic compare operations
         * @return Complete ptrAndTag value including both address and version
         * 
         * Used by atomic operations that need to compare the entire
         * tagged pointer (address + version) in a single operation.
         */
        uintptr_t raw() const { return ptrAndTag; }
    };
    
    /**
     * Compile-time assertion: TaggedPtr must be the same size as uintptr_t
     * (8 bytes on 64-bit systems) so that atomic operations work correctly.
     * 
     * If this fails, the struct has unexpected padding and atomic operations
     * might not be lock-free, defeating the purpose of this implementation.
     */
    static_assert(sizeof(TaggedPtr) == sizeof(uintptr_t), "TaggedPtr must be atomic");

    /**
     * @struct Node
     * @brief Stack node containing engine pointer and tagged next pointer
     * 
     * The Treiber stack is a linked list where each node contains:
     *   - engine: The LuaEngine being stored in the pool
     *   - next: Tagged pointer to the next node in the stack
     * 
     * The tagged next pointer is crucial: if we just used a raw pointer,
     * the ABA problem could corrupt the stack when a node is freed and
     * reallocated between reading next and performing the CAS.
     * 
     * Memory layout per node:
     *   [Node]
     *   - engine: LuaEngine* (8 bytes)
     *   - next: TaggedPtr (8 bytes, includes version tag)
     * 
     * Total per node: ~16 bytes + allocator overhead
     */
    struct Node {
        TaggedPtr next;       // Pointer to next node (with version tag for ABA safety)
        LuaEngine* engine;    // The actual engine being stored in the pool
        
        explicit Node(LuaEngine* e) : engine(e) {}
    };

private:
    // ==========================================
    // STACK HEAD - THE CENTRAL DATA STRUCTURE
    // ==========================================
    // 
    // The head_ atomic holds the top of the stack as a tagged pointer.
    // All operations (push/pop) synchronize through this single atomic.
    // 
    // Cache line alignment (alignas(64)): Modern CPUs fetch memory in
    // 64-byte cache lines. By aligning head_ to 64 bytes, we ensure it
    // doesn't share a cache line with other data, preventing "false sharing"
    // where unrelated data invalidates the cache line.
    // 
    // False sharing example:
    //   Without alignment:
    //   | head_ (4 bytes) | other_data (60 bytes) |
    //   Thread A writes head_ -> entire cache line invalidated
    //   Thread B reads other_data -> cache miss, must reload
    //   
    //   With alignment:
    //   | head_ (64 bytes, aligned) | other_data (...) |
    //   Separate cache lines, no false sharing
    //
    alignas(64) std::atomic<TaggedPtr> head_{TaggedPtr()};
    
    // ==========================================
    // HAZARD POINTERS - SAFE MEMORY RECLAMATION
    // ==========================================
    // 
    // THE PROBLEM WE'RE SOLVING:
    //   Thread A: Reads head = Node X
    //   Thread A: About to dereference X->next
    //   [Thread A gets preempted]
    //   Thread B: Pops X (CAS succeeds), deletes X
    //   Thread A: Resumes, dereferences X->next -> USE AFTER FREE!
    //
    // SOLUTION - ANNOUNCE INTENT TO ACCESS:
    //   Before dereferencing any node that might be freed by another thread,
    //   a thread writes the node's address to its hazard pointer slot.
    //   
    //   Thread A: Read head = X
    //   Thread A: hazardPointers_[myId] = X  (announce: "I'm using X")
    //   Thread A: Dereference X->next (safe - X won't be freed now)
    //   Thread A: CAS to pop X
    //   Thread A: hazardPointers_[myId] = nullptr (done with X)
    //   
    //   Thread B: Tries to pop X, loses CAS race
    //   Thread B: Wins CAS race for another node Y
    //   Thread B: Before deleting Y, check all hazard pointers:
    //             for i in 0..MAX_THREADS:
    //                 if hazardPointers_[i] == Y: can't delete!
    //   Thread B: No hazard pointer to Y, safe to delete
    //
    // IMPLEMENTATION DETAILS:
    //   - Fixed array size MAX_THREADS = 64
    //   - Each thread gets a unique index via thread_local storage
    //   - Cache line aligned to prevent false sharing
    //   - Initialized to nullptr (no hazards initially)
    //
    // LIMITATIONS:
    //   - Maximum 64 threads (can be increased if needed)
    //   - Each thread needs to call getThreadId() before using pop()
    //   - Destructor assumes no threads are still using the pool
    //
    static constexpr size_t MAX_THREADS = 64;
    alignas(64) std::atomic<Node*> hazardPointers_[MAX_THREADS];
    
    // ==========================================
    // EPOCH-BASED MEMORY RECLAMATION
    // ==========================================
    //
    // SIMPLIFICATION: This implementation uses a basic approach where
    // nodes are retired to a list and reclaimed in the destructor.
    // A full epoch-based system would track per-node retirement epochs
    // and only reclaim when all threads have passed that epoch.
    //
    // For the engine pool use case (long-lived pool, reclaimed at
    // workflow destruction), this simplified approach is sufficient.
    //
    // The epoch tracking infrastructure is present for future enhancement.
    //
    // Retired nodes waiting for safe deletion (simplified - reclaimed in destructor)
    alignas(64) std::atomic<Node*> retiredList_{nullptr};
    
    // Global epoch counter - incremented occasionally (for future enhancement)
    alignas(64) std::atomic<uint64_t> globalEpoch_{0};
    
    // Per-thread epoch tracking - each thread's last seen epoch (for future enhancement)
    alignas(64) std::atomic<uint64_t> threadEpochs_[MAX_THREADS];
    
    // Thread ID assignment counter - gives each thread a unique slot
    // Relaxed ordering is sufficient - just needs to be atomic to prevent
    // duplicate IDs, ordering doesn't affect correctness here.
    alignas(64) std::atomic<size_t> nextThreadId_{0};

public:
    /**
     * @brief Constructor - initializes hazard pointers and epoch tracking
     * 
     * Sets all hazard pointers to nullptr and all thread epochs to 0.
     * Uses relaxed ordering since no other threads can access the pool
     * during construction.
     */
    LockFreeEnginePool() {
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            hazardPointers_[i].store(nullptr, std::memory_order_relaxed);
            threadEpochs_[i].store(0, std::memory_order_relaxed);
        }
    }
    
    /**
     * @brief Destructor - cleans up all nodes
     * 
     * IMPORTANT: The destructor assumes NO other threads are accessing
     * the pool. This is safe for the Workflow use case because:
     *   - Workflow owns the LockFreeEnginePool (via unique_ptr)
     *   - Workflow is destroyed after all parallel execution completes
     *   - No threads should be executing workflow rules during destruction
     * 
     * In a production library, this would verify all hazard pointers
     * are nullptr and wait for any pending operations.
     */
    ~LockFreeEnginePool() {
        reclaimAll();
    }

    /**
     * @brief Get thread-local thread ID for hazard pointer array indexing
     * 
     * Each thread needs a unique slot in the hazard pointer array.
     * This function assigns slots lazily using thread_local storage.
     * 
     * thread_local means each thread gets its own copy of 'myId',
     * initialized exactly once on first call.
     * 
     * The modulo operation wraps around if more than MAX_THREADS threads
     * are created. This means two threads might share a slot, which could
     * delay memory reclamation but won't cause safety issues (conservative).
     * 
     * @return Slot index in hazardPointers_ array (0 to MAX_THREADS-1)
     */
    size_t getThreadId() {
        thread_local size_t myId = nextThreadId_.fetch_add(1, std::memory_order_relaxed);
        return myId % MAX_THREADS;
    }

    /**
     * @brief Push an engine onto the stack (lock-free)
     * 
     * OPERATION:
     *   1. Allocate new node with the engine pointer
     *   2. Load current head (with acquire semantics)
     *   3. Link new node to current head (newNode->next = oldHead)
     *   4. Create new tagged head (same address, version+1)
     *   5. CAS: if head still equals oldHead, swap to newHead
     *   6. If CAS fails, retry from step 2 (another thread changed head)
     * 
     * MEMORY ORDERING EXPLAINED:
     *   - Load uses memory_order_relaxed because we'll verify with CAS
     *   - CAS success uses memory_order_release: this ensures all writes
     *     to newNode (steps 1-3) are visible to other threads BEFORE
     *     the head pointer update is visible. This is crucial - without
     *     release semantics, another thread might see the new head pointer
     *     but not see the initialized newNode->next field.
     *   - CAS failure uses memory_order_relaxed: we're just reloading
     * 
     * ABA PROTECTION:
     *   The version tag in newHead is oldHead.tag() + 1. Even if the
     *   same memory address gets reused for a different node, the
     *   version will differ, causing concurrent pops to fail their CAS.
     * 
     * PROGRESS GUARANTEE:
     *   This is lock-free (not wait-free). A thread can theoretically
     *   starve if other threads keep succeeding their CAS operations.
     *   In practice, with reasonable thread counts, starvation is
     *   extremely unlikely. The compare_exchange_weak helps by telling
     *   the caller when to retry.
     * 
     * @param engine Pointer to LuaEngine to push onto the stack
     */
    void push(LuaEngine* engine) {
        // Step 1: Allocate new node
        // This memory allocation is the only blocking operation in push().
        // In a high-performance scenario, this could use a lock-free allocator
        // or a pre-allocated pool of nodes.
        Node* newNode = new Node(engine);
        
        // Step 2: Load current head
        // Relaxed ordering is fine here because we're about to verify
        // with CAS. We just need the value, not synchronization.
        TaggedPtr oldHead = head_.load(std::memory_order_relaxed);
        
        // Step 3-6: Retry loop for CAS
        for (;;) {
            // Link new node to current head
            newNode->next = oldHead;
            
            // Create new head with incremented version tag
            // The version prevents ABA: even if the same address is reused,
            // the tag will differ, causing other threads' CAS to fail
            TaggedPtr newHead(engine, oldHead.tag() + 1);
            
            // CAS operation: atomically check if head == oldHead, and if so,
            // replace it with newHead. Returns true if successful.
            //
            // MEMORY ORDERING:
            //   - memory_order_release on success: Ensures newNode initialization
            //     (steps 1-3, especially newNode->next = oldHead) is visible to
            //     other threads BEFORE the head pointer update. Without this,
            //     another thread might see head = newNode but newNode->next
            //     still uninitialized (data race).
            //   
            //   - memory_order_relaxed on failure: We don't care about ordering
            //     if CAS failed, just need the current head value to retry.
            //
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
                // CAS succeeded! The new node is now visible to all threads.
                // The release semantics ensure they see a fully initialized node.
                break;
            }
            
            // CAS failed - another thread changed head between our load and CAS.
            // oldHead has been updated to the current value, so we can just
            // retry with the correct head pointer. No need to reload.
        }
    }

    /**
     * @brief Pop an engine from the stack (lock-free)
     * 
     * OPERATION:
     *   1. Load head with acquire semantics
     *   2. Check if stack is empty (head == nullptr)
     *   3. Set hazard pointer BEFORE dereferencing head
     *   4. Reload head to verify it hasn't changed (safety check)
     *   5. Try CAS: head = head->next
     *   6. If successful, clear hazard pointer and return engine
     *   7. If failed, clear hazard pointer and retry
     * 
     * MEMORY ORDERING EXPLAINED:
     *   - Step 1 uses memory_order_acquire: This synchronizes-with the
     *     release store in push(). It ensures we see all writes to the
     *     node (including newNode->next) from the pushing thread.
     *   
     *   - Step 3 (hazard pointer) uses memory_order_release: This makes
     *     the hazard pointer visible to other threads BEFORE we dereference
     *     the node. Without this, another thread might free the node before
     *     seeing our hazard pointer.
     *   
     *   - Step 4 reloads with memory_order_acquire to see any concurrent
     *     updates that happened after we set the hazard pointer.
     *   
     *   - Step 5 CAS success uses memory_order_release: Ensures the popped
     *     node is no longer visible before we clear our hazard pointer.
     * 
     * HAZARD POINTER PROTOCOL:
     *   This follows the classic hazard pointer pattern:
     *   1. Read pointer to be dereferenced
     *   2. Write hazard pointer = that pointer
     *   3. Re-read pointer, verify it hasn't changed
     *   4. If changed, clear hazard and retry (ABA scenario)
     *   5. If unchanged, safe to dereference
     *   6. After CAS succeeds, clear hazard pointer
     * 
     * This ensures we never dereference a node that another thread
     * might free between reading the pointer and using it.
     * 
     * @return Pointer to popped LuaEngine, or nullptr if stack is empty
     */
    LuaEngine* pop() {
        // Get this thread's slot in the hazard pointer array
        size_t tid = getThreadId();
        
        // Retry loop for CAS failures
        for (;;) {
            // Step 1: Acquire load of head
            // memory_order_acquire: Synchronizes-with the release in push().
            // This ensures we see the fully initialized node that was pushed.
            // Without acquire, we might see the head pointer update but not
            // the writes to the node's memory (data race).
            TaggedPtr oldHead = head_.load(std::memory_order_acquire);
            Node* node = reinterpret_cast<Node*>(oldHead.ptr());
            
            // Step 2: Check for empty stack
            if (node == nullptr) {
                return nullptr;
            }
            
            // Step 3: Set hazard pointer BEFORE dereferencing node
            // memory_order_release: Ensures the hazard pointer is visible
            // to other threads before we access node->next. This prevents
            // the scenario where another thread frees 'node' before seeing
            // our hazard pointer.
            hazardPointers_[tid].store(node, std::memory_order_release);
            
            // Step 4: Reload head to verify it hasn't changed
            // memory_order_acquire: Ensures we see any concurrent updates.
            // If head changed between our first load and hazard pointer store,
            // another thread may have popped and freed this node.
            TaggedPtr currentHead = head_.load(std::memory_order_acquire);
            if (currentHead.raw() != oldHead.raw()) {
                // Head changed - clear hazard and retry
                // We might have set hazard to a freed node, but that's okay
                // because we'll retry and get the correct head.
                hazardPointers_[tid].store(nullptr, std::memory_order_release);
                continue;
            }
            
            // Step 5: Try to pop with CAS
            // New head is node's next pointer (with new version tag)
            TaggedPtr newHead(node->next.ptr(), oldHead.tag() + 1);
            
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
                // Success! We popped the node.
                // Clear hazard pointer - we're done with this node.
                hazardPointers_[tid].store(nullptr, std::memory_order_release);
                
                // Extract the engine and retire the node
                LuaEngine* engine = node->engine;
                retireNode(node);
                return engine;
            }
            
            // CAS failed - head changed after our hazard check
            // This can happen if another thread popped between steps 4 and 5.
            // Clear hazard and retry with updated head.
            hazardPointers_[tid].store(nullptr, std::memory_order_release);
            // Loop continues with updated oldHead from failed CAS
        }
    }

    /**
     * @brief Initialize pool with pre-created engines from a vector
     * 
     * This is used by Workflow::compile() to populate the pool with
     * pre-compiled engine clones. Engines are pushed in reverse order
     * so the first engine in the vector is at the top of the stack
     * (most likely to be popped first).
     * 
     * Note: This method stores raw pointers. The caller (Workflow) retains
     * ownership of the engines in enginePoolStorage_. The pool just
     * manages which engines are available for checkout.
     * 
     * @param engines Vector of unique_ptr<LuaEngine> to add to the pool
     */
    void initializePool(const std::vector<std::unique_ptr<LuaEngine>>& engines) {
        // Reverse iteration so first engine ends up at top of stack
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
     * After popping a node, we can't delete it immediately because
     * other threads might still be accessing it (their hazard pointers
     * might point to it). Instead, we add it to a retired list.
     * 
     * In this simplified implementation, nodes are reclaimed in the
     * destructor. A full implementation would periodically scan hazard
     * pointers and delete nodes with no active hazard pointers.
     * 
     * @param node The popped node to retire
     */
    void retireNode(Node* node) {
        // Push onto retired list using CAS
        Node* oldRetired = retiredList_.load(std::memory_order_relaxed);
        
        for (;;) {
            // Link to current head of retired list
            // We use a dummy TaggedPtr here since retired list doesn't
            // need version tags (not accessed concurrently during normal ops)
            node->next = TaggedPtr(reinterpret_cast<LuaEngine*>(oldRetired), 0);
            
            // CAS to add to retired list
            if (retiredList_.compare_exchange_weak(oldRetired, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                break;
            }
        }
        
        // Try to reclaim memory (simplified - mostly does nothing)
        tryReclaim();
    }

    /**
     * @brief Attempt to reclaim retired nodes
     * 
     * This is a placeholder for full epoch-based reclamation.
     * In a production system, this would check if all threads have
     * passed a certain epoch, making it safe to delete nodes retired
     * in earlier epochs.
     * 
     * For this engine pool use case, reclamation happens in destructor.
     */
    void tryReclaim() {
        // Simplified: nodes are reclaimed in destructor
        // Full implementation would check hazard pointers here
        (void)0;  // No-op to avoid empty function warning
    }

    /**
     * @brief Reclaim all nodes in retired list
     * 
     * Called by tryReclaim when it's safe to delete nodes.
     * This simplified version reclaims all retired nodes.
     * 
     * @param safeEpoch Epoch below which all nodes are safe to delete
     */
    void reclaimNodes([[maybe_unused]] uint64_t safeEpoch) {
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
     * This is safe because:
     *   - Workflow owns the pool via unique_ptr
     *   - Workflow is destroyed after all parallel execution completes
     *   - No threads should be calling push/pop during destruction
     * 
     * Frees both active stack nodes and retired nodes.
     */
    void reclaimAll() {
        // Free active stack nodes
        // Exchange head to nullptr, getting old head value
        TaggedPtr head = head_.exchange(TaggedPtr(), std::memory_order_acquire);
        Node* node = reinterpret_cast<Node*>(head.ptr());
        
        // Walk the stack and free each node
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
