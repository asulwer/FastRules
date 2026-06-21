// stress_runner.hpp
// Lightweight harness for FastRules stress tests.
// No third-party dependency; uses only the standard library and the engine's
// memory reporter.

#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

namespace fastrules::stress {

struct StressConfig {
    // Default values give a quick smoke run. Override via command line.
    double durationSeconds = 2.0;
    size_t iterations = 0;            // If > 0, overrides duration.
    size_t threads = 1;
    size_t rules = 20;
    size_t parameters = 5;
    size_t autoResetThresholdKB = 0;  // 0 disables auto-reset checks.
    bool verbose = true;

    static StressConfig fromArgs(int argc, char* argv[]) {
        StressConfig cfg;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto next = [&](size_t& out) {
                if (i + 1 < argc) out = static_cast<size_t>(std::atoll(argv[++i]));
            };
            auto nextDouble = [&](double& out) {
                if (i + 1 < argc) out = std::atof(argv[++i]);
            };
            if (arg == "--duration") nextDouble(cfg.durationSeconds);
            else if (arg == "--iterations") next(cfg.iterations);
            else if (arg == "--threads") next(cfg.threads);
            else if (arg == "--rules") next(cfg.rules);
            else if (arg == "--parameters") next(cfg.parameters);
            else if (arg == "--auto-reset-kb") next(cfg.autoResetThresholdKB);
            else if (arg == "--quiet") cfg.verbose = false;
        }
        return cfg;
    }
};

struct StressResult {
    std::string name;
    size_t ops = 0;
    double elapsedMs = 0.0;
    double opsPerSecond = 0.0;
    size_t peakMemKB = 0;
    size_t finalMemKB = 0;
    size_t errors = 0;
    std::string note;

    void print(std::ostream& os = std::cout) const {
        os << std::left << std::setw(30) << name
           << " ops=" << std::setw(10) << ops
           << " time=" << std::fixed << std::setprecision(1) << std::setw(8) << elapsedMs << "ms"
           << " ops/s=" << std::setw(12) << std::setprecision(0) << opsPerSecond
           << " peakMem=" << std::setw(8) << peakMemKB << "KB"
           << " finalMem=" << std::setw(8) << finalMemKB << "KB";
        if (errors) os << " errors=" << errors;
        if (!note.empty()) os << " [" << note << "]";
        os << "\n";
    }
};

class MemorySampler {
public:
    using ProbeFn = std::function<size_t()>;

    explicit MemorySampler(ProbeFn probe = nullptr)
        : probe_(std::move(probe)), peakKB_(0) {}

    void probe() {
        size_t kb = probe_ ? probe_() : 0;
        std::lock_guard<std::mutex> lock(mutex_);
        if (kb > peakKB_) peakKB_ = kb;
        lastKB_ = kb;
    }

    [[nodiscard]] size_t peakKB() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return peakKB_;
    }

    [[nodiscard]] size_t lastKB() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastKB_;
    }

private:
    ProbeFn probe_;
    mutable std::mutex mutex_;
    size_t peakKB_ = 0;
    size_t lastKB_ = 0;
};

class StressRunner {
public:
    explicit StressRunner(std::function<size_t()> memProbe = nullptr)
        : memory_(std::move(memProbe)) {}

    // Run a workload for a fixed duration or fixed iteration count.
    StressResult run(const std::string& name,
                     const StressConfig& cfg,
                     const std::function<void(size_t iterationIndex)>& work) {
        StressResult result;
        result.name = name;

        memory_.probe();
        auto start = std::chrono::high_resolution_clock::now();

        if (cfg.iterations > 0) {
            for (size_t i = 0; i < cfg.iterations; ++i) {
                doWork(i, work);
            }
            result.ops = cfg.iterations;
        } else {
            size_t i = 0;
            auto deadline = start + std::chrono::duration<double>(cfg.durationSeconds);
            while (std::chrono::high_resolution_clock::now() < deadline) {
                doWork(i++, work);
            }
            result.ops = i;
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        result.opsPerSecond = result.elapsedMs > 0 ? (result.ops / (result.elapsedMs / 1000.0)) : 0.0;
        result.peakMemKB = memory_.peakKB();
        result.finalMemKB = memory_.lastKB();
        return result;
    }

    // Run the same workload from N worker threads concurrently.
    StressResult runConcurrent(const std::string& name,
                               const StressConfig& cfg,
                               const std::function<void(size_t workerId, size_t iterationIndex)>& work) {
        StressResult result;
        result.name = name + " (threads=" + std::to_string(cfg.threads) + ")";

        memory_.probe();
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::future<size_t>> futures;
        std::atomic<size_t> totalOps{0};
        std::atomic<size_t> totalErrors{0};

        for (size_t t = 0; t < cfg.threads; ++t) {
            futures.push_back(std::async(std::launch::async, [&, t]() {
                size_t localOps = 0;
                size_t localErrors = 0;
                if (cfg.iterations > 0) {
                    size_t perThread = cfg.iterations / cfg.threads;
                    size_t remainder = (t == cfg.threads - 1) ? (cfg.iterations % cfg.threads) : 0;
                    for (size_t i = 0; i < perThread + remainder; ++i) {
                        try {
                            work(t, i);
                        } catch (...) {
                            ++localErrors;
                        }
                        ++localOps;
                    }
                } else {
                    auto deadline = std::chrono::high_resolution_clock::now()
                                    + std::chrono::duration<double>(cfg.durationSeconds);
                    size_t i = 0;
                    while (std::chrono::high_resolution_clock::now() < deadline) {
                        try {
                            work(t, i++);
                        } catch (...) {
                            ++localErrors;
                        }
                        ++localOps;
                    }
                }
                totalOps.fetch_add(localOps);
                totalErrors.fetch_add(localErrors);
                return localOps;
            }));
        }

        for (auto& f : futures) {
            f.wait();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        result.ops = totalOps.load();
        result.errors = totalErrors.load();
        result.opsPerSecond = result.elapsedMs > 0 ? (result.ops / (result.elapsedMs / 1000.0)) : 0.0;
        result.peakMemKB = memory_.peakKB();
        result.finalMemKB = memory_.lastKB();
        return result;
    }

private:
    MemorySampler memory_;

    void doWork(size_t i, const std::function<void(size_t)>& work) {
        work(i);
        if ((i & 0x7F) == 0) {  // sample memory every 128 ops
            memory_.probe();
        }
    }
};

inline int runSuite(const std::string& suiteName,
                    const std::vector<std::function<StressResult(const StressConfig&)>>& scenarios,
                    int argc,
                    char* argv[]) {
    auto cfg = StressConfig::fromArgs(argc, argv);
    if (cfg.verbose) {
        std::cout << "=== FastRules stress suite: " << suiteName << " ===\n"
                  << "duration=" << cfg.durationSeconds << "s"
                  << " iterations=" << cfg.iterations
                  << " threads=" << cfg.threads
                  << " rules=" << cfg.rules
                  << " parameters=" << cfg.parameters << "\n\n";
    } else {
        spdlog::set_level(spdlog::level::off);
    }

    bool anyError = false;
    for (const auto& scenario : scenarios) {
        auto result = scenario(cfg);
        if (cfg.verbose) result.print();
        if (result.errors > 0) anyError = true;
    }

    if (cfg.verbose) std::cout << "\n";
    return anyError ? 1 : 0;
}

} // namespace fastrules::stress
