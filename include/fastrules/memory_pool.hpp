/**
 * @file memory_pool.hpp
 * @brief Memory pooling for FastRules objects
 * 
 * Provides memory pools for frequently allocated objects to reduce
 * allocation overhead and improve performance.
 */

#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

namespace fastrules {

/**
 * @brief Generic memory pool for objects of type T
 * 
 * Thread-safe memory pool that pre-allocates objects and reuses them
 * to avoid frequent allocations and deallocations.
 */
template<typename T>
class MemoryPool {
private:
    std::queue<std::unique_ptr<T>> pool_;
    mutable std::mutex mutex_;
    size_t maxSize_;
    std::atomic<size_t> allocatedCount_{0};
    std::atomic<size_t> releasedCount_{0};

public:
    /**
     * @brief Construct memory pool
     * 
     * @param maxSize Maximum number of objects to keep in pool
     */
    explicit MemoryPool(size_t maxSize = 1000) : maxSize_(maxSize) {}

    /**
     * @brief Acquire object from pool or create new one
     * 
     * @return Unique pointer to object
     */
    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_.empty()) {
            auto obj = std::move(pool_.front());
            pool_.pop();
            return obj;
        }
        
        allocatedCount_++;
        return std::make_unique<T>();
    }

    /**
     * @brief Release object back to pool
     * 
     * @param obj Object to release
     */
    void release(std::unique_ptr<T> obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.size() < maxSize_) {
            // Clear object state before returning to pool (if it's a container)
            // For generic objects, we just return them to the pool
            pool_.push(std::move(obj));
        }
        // If pool is full, obj will be destroyed when it goes out of scope
        releasedCount_++;
    }

    /**
     * @brief Pre-allocate objects in pool
     * 
     * @param count Number of objects to pre-allocate
     */
    void preallocate(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count && pool_.size() < maxSize_; ++i) {
            pool_.push(std::make_unique<T>());
            allocatedCount_++;
        }
    }

    /**
     * @brief Get pool statistics
     * 
     * @return Pair of (pool size, total allocated)
     */
    std::pair<size_t, size_t> getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {pool_.size(), allocatedCount_.load()};
    }

    /**
     * @brief Clear pool
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
    }
};

/**
 * @brief Specialized memory pool for vectors
 * 
 * Optimized pool for std::vector objects that can be cleared and reused.
 */
template<typename T>
class VectorPool {
private:
    std::queue<std::unique_ptr<std::vector<T>>> pool_;
    mutable std::mutex mutex_;
    size_t maxSize_;
    size_t initialCapacity_;
    std::atomic<size_t> allocatedCount_{0};
    std::atomic<size_t> releasedCount_{0};

public:
    /**
     * @brief Construct vector pool
     * 
     * @param maxSize Maximum number of vectors to keep in pool
     * @param initialCapacity Initial capacity for new vectors
     */
    explicit VectorPool(size_t maxSize = 1000, size_t initialCapacity = 10) 
        : maxSize_(maxSize), initialCapacity_(initialCapacity) {}

    /**
     * @brief Acquire vector from pool
     * 
     * @return Unique pointer to cleared vector
     */
    std::unique_ptr<std::vector<T>> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_.empty()) {
            auto vec = std::move(pool_.front());
            pool_.pop();
            vec->clear();  // Clear existing data
            return vec;
        }
        
        allocatedCount_++;
        auto vec = std::make_unique<std::vector<T>>();
        vec->reserve(initialCapacity_);
        return vec;
    }

    /**
     * @brief Release vector back to pool
     * 
     * @param vec Vector to release
     */
    void release(std::unique_ptr<std::vector<T>> vec) {
        if (!vec) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.size() < maxSize_) {
            vec->clear();  // Clear data before returning to pool
            pool_.push(std::move(vec));
        }
        // If pool is full, vec will be destroyed when it goes out of scope
        releasedCount_++;
    }

    /**
     * @brief Pre-allocate vectors in pool
     * 
     * @param count Number of vectors to pre-allocate
     */
    void preallocate(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count && pool_.size() < maxSize_; ++i) {
            auto vec = std::make_unique<std::vector<T>>();
            vec->reserve(initialCapacity_);
            pool_.push(std::move(vec));
            allocatedCount_++;
        }
    }

    /**
     * @brief Get pool statistics
     * 
     * @return Pair of (pool size, total allocated)
     */
    std::pair<size_t, size_t> getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {pool_.size(), allocatedCount_.load()};
    }

    /**
     * @brief Clear pool
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
    }
};

/**
 * @brief FastRules memory manager
 * 
 * Centralized memory management for FastRules objects.
 */
class MemoryManager {
private:
    // Pools for common FastRules objects
    std::unique_ptr<MemoryPool<class RuleContext>> contextPool_;
    std::unique_ptr<VectorPool<class RuleResult>> resultVectorPool_;
    
    // Singleton instance
    static std::unique_ptr<MemoryManager> instance_;
    static std::mutex instanceMutex_;

public:
    /**
     * @brief Construct memory manager
     */
    MemoryManager();

    /**
     * @brief Get singleton instance
     * 
     * @return Memory manager instance
     */
    static MemoryManager& getInstance();

    /**
     * @brief Acquire RuleContext from pool
     * 
     * @return Unique pointer to RuleContext
     */
    std::unique_ptr<class RuleContext> acquireContext();

    /**
     * @brief Release RuleContext back to pool
     * 
     * @param context Context to release
     */
    void releaseContext(std::unique_ptr<class RuleContext> context);

    /**
     * @brief Acquire RuleResult vector from pool
     * 
     * @return Unique pointer to RuleResult vector
     */
    std::unique_ptr<std::vector<class RuleResult>> acquireResultVector();

    /**
     * @brief Release RuleResult vector back to pool
     * 
     * @param results Vector to release
     */
    void releaseResultVector(std::unique_ptr<std::vector<class RuleResult>> results);

    /**
     * @brief Pre-allocate objects in pools
     */
    void preallocate();

    /**
     * @brief Get memory manager statistics
     */
    void getStats(size_t& contextPoolSize, size_t& contextAllocated,
                  size_t& resultPoolSize, size_t& resultAllocated) const;
};

} // namespace fastrules