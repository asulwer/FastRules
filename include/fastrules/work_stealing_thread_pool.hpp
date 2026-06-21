/**
 * @file work_stealing_thread_pool.hpp
 * @brief Work-stealing thread pool implementation
 * 
 * Thread pool that uses work-stealing queues to improve load balancing.
 * Each thread has its own local queue, and when idle, threads steal work
 * from other threads' queues.
 */

#pragma once

#include "fastrules/simple_work_stealing_queue.hpp"

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <random>
#include <chrono>

namespace fastrules {

/**
 * @brief Work-stealing thread pool
 * 
 * Implements a thread pool with work-stealing capabilities:
 * - Each thread has a local task queue
 * - When a thread is idle, it tries to steal work from other threads
 * - Global queue for tasks submitted from outside
 */
class WorkStealingThreadPool {
private:
    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<SimpleWorkStealingQueue<std::function<void()>>>> localQueues_;
    std::atomic<bool> stop_;
    std::atomic<uint32_t> idleWorkers_{0};
    
public:
    /**
     * @brief Construct work-stealing thread pool
     * 
     * @param numThreads Number of worker threads
     */
    explicit WorkStealingThreadPool(size_t numThreads);
    
    /**
     * @brief Destructor - shuts down thread pool
     */
    ~WorkStealingThreadPool();
    
    /**
     * @brief Enqueue task for execution
     * 
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param func Function to execute
     * @param args Arguments to function
     * @return Future for result
     */
    template<typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        using return_type = std::invoke_result_t<Func, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        // Push task to a random local queue for load balancing.
        // The distribution is not static so a thread using multiple pools always
        // indexes within the current pool's range.
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<size_t> dist(0, localQueues_.size() - 1);

        size_t queueIndex = dist(rng);
        localQueues_[queueIndex]->push([task]() { (*task)(); });
        
        return result;
    }

private:
    /**
     * @brief Worker thread loop
     * 
     * @param workerIndex Index of this worker thread
     */
    void workerLoop(size_t workerIndex);
};

} // namespace fastrules