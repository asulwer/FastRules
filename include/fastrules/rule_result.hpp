#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <any>
#include <chrono>
#include <stdexcept>

namespace fastrules {

class Rule;  // forward

struct RuleMetrics {
    uint64_t evaluationCount = 0;
    std::chrono::nanoseconds totalExecutionTime{0};
    uint64_t failureCount = 0;
    std::chrono::steady_clock::time_point lastExecuted;

    [[nodiscard]] std::chrono::nanoseconds averageExecutionTime() const noexcept {
        return evaluationCount > 0 
            ? std::chrono::nanoseconds(totalExecutionTime.count() / static_cast<std::int64_t>(evaluationCount)) 
            : std::chrono::nanoseconds{0};
    }

    [[nodiscard]] double failureRate() const noexcept {
        return evaluationCount > 0 ? static_cast<double>(failureCount) / evaluationCount : 0.0;
    }
};

class RuleException : public std::runtime_error {
public:
    explicit RuleException(const std::string& msg) : std::runtime_error(msg) {}
    explicit RuleException(std::string&& msg) : std::runtime_error(std::move(msg)) {}
};

class RuleCompilationException : public RuleException {
public:
    explicit RuleCompilationException(const std::string& msg) : RuleException(msg) {}
    explicit RuleCompilationException(std::string&& msg) : RuleException(std::move(msg)) {}
};

class RuleValidationException : public RuleException {
public:
    explicit RuleValidationException(const std::string& msg) : RuleException(msg) {}
    explicit RuleValidationException(std::string&& msg) : RuleException(std::move(msg)) {}
};

class RuleExecutionException : public RuleException {
public:
    explicit RuleExecutionException(const std::string& msg) : RuleException(msg) {}
    explicit RuleExecutionException(std::string&& msg) : RuleException(std::move(msg)) {}
};

class RuleTimeoutException : public RuleExecutionException {
public:
    explicit RuleTimeoutException(const std::string& msg) : RuleExecutionException(msg) {}
    explicit RuleTimeoutException(std::string&& msg) : RuleExecutionException(std::move(msg)) {}
};

class RateLimitException : public RuleException {
public:
    explicit RateLimitException(const std::string& msg) : RuleException(msg) {}
    explicit RateLimitException(std::string&& msg) : RuleException(std::move(msg)) {}
};

struct RuleResult {
    std::string ruleId;
    bool success = false;
    bool skipped = false;  // True if rule was inactive and skipped
    std::optional<std::any> value;
    std::optional<RuleException> exception;
    std::vector<RuleResult> childResults;
    RuleMetrics metrics;
    std::chrono::steady_clock::time_point executedAt;

    RuleResult() = default;
    ~RuleResult() = default;
    RuleResult(RuleResult&&) noexcept = default;
    RuleResult& operator=(RuleResult&&) noexcept = default;
    RuleResult(const RuleResult&) = default;
    RuleResult& operator=(const RuleResult&) = default;

    [[nodiscard]] bool isSuccess() const noexcept { return success && !exception.has_value() && !skipped; }
    [[nodiscard]] bool isFullySuccessful() const noexcept;
};

} // namespace fastrules
