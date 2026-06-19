#include "fastrules/memory_pool.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"

#include <memory>

namespace fastrules {

// Static member definitions
std::unique_ptr<MemoryManager> MemoryManager::instance_;
std::mutex MemoryManager::instanceMutex_;

MemoryManager::MemoryManager() 
    : contextPool_(std::make_unique<MemoryPool<RuleContext>>(1000))
    , resultVectorPool_(std::make_unique<VectorPool<RuleResult>>(1000, 10)) {
}

MemoryManager& MemoryManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    if (!instance_) {
        instance_ = std::make_unique<MemoryManager>();
    }
    return *instance_;
}

std::unique_ptr<RuleContext> MemoryManager::acquireContext() {
    return contextPool_->acquire();
}

void MemoryManager::releaseContext(std::unique_ptr<RuleContext> context) {
    if (context) {
        context->clear();  // Clear context state before returning to pool
        contextPool_->release(std::move(context));
    }
}

std::unique_ptr<std::vector<RuleResult>> MemoryManager::acquireResultVector() {
    return resultVectorPool_->acquire();
}

void MemoryManager::releaseResultVector(std::unique_ptr<std::vector<RuleResult>> results) {
    resultVectorPool_->release(std::move(results));
}

void MemoryManager::preallocate() {
    contextPool_->preallocate(100);
    resultVectorPool_->preallocate(100);
}

void MemoryManager::getStats(size_t& contextPoolSize, size_t& contextAllocated,
                             size_t& resultPoolSize, size_t& resultAllocated) const {
    auto contextStats = contextPool_->getStats();
    contextPoolSize = contextStats.first;
    contextAllocated = contextStats.second;
    
    auto resultStats = resultVectorPool_->getStats();
    resultPoolSize = resultStats.first;
    resultAllocated = resultStats.second;
}

} // namespace fastrules