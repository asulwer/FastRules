#include "fastrules/work_stealing_thread_pool.hpp"
#include "fastrules/logger.hpp"

#include <algorithm>
#include <random>
#include <chrono>

namespace fastrules {

WorkStealingThreadPool::WorkStealingThreadPool(size_t numThreads) 
    : localQueues_(numThreads)
    , stop_(false) {
    
    // Initialize local queues
    for (size_t i = 0; i < numThreads; ++i) {
        localQueues_[i] = std::make_unique<SimpleWorkStealingQueue<std::function<void()>>>();
    }
    
    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this, i] {
            workerLoop(i);
        });
    }
}

WorkStealingThreadPool::~WorkStealingThreadPool() {
    stop_ = true;
    
    // Wait for all worker threads to finish
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void WorkStealingThreadPool::workerLoop(size_t workerIndex) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, localQueues_.size() - 1);
    
    while (!stop_.load(std::memory_order_acquire)) {
        std::function<void()> task;
        bool foundTask = false;
        
        // Try to pop from own local queue first (LIFO)
        if (auto localTask = localQueues_[workerIndex]->pop()) {
            task = *localTask;
            foundTask = true;
        }
        
        // If no task in own queue, try to steal from others (FIFO)
        if (!foundTask) {
            idleWorkers_.fetch_add(1, std::memory_order_release);
            
            // Try to steal from random queues
            for (size_t attempts = 0; attempts < localQueues_.size() * 2 && !foundTask; ++attempts) {
                size_t victim = dis(gen);
                if (victim != workerIndex) {
                    if (auto stolenTask = localQueues_[victim]->steal()) {
                        task = *stolenTask;
                        foundTask = true;
                        break;
                    }
                }
            }
            
            idleWorkers_.fetch_sub(1, std::memory_order_release);
        }
        
        // Execute task if found
        if (foundTask) {
            try {
                if (task) {
                    task();
                }
            } catch (...) {
                // Log exception if logger available
                try {
                    auto log = fastrules::logger();
                    if (log) {
                        log->error("Exception in work-stealing thread pool task");
                    }
                } catch (...) {
                    // Ignore logging errors
                }
            }
        } else {
            // No work available, sleep briefly to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

} // namespace fastrules