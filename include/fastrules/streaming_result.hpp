#pragma once

#include "rule_result.hpp"
#include <optional>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

namespace fastrules {

// Streaming result — yields RuleResults as they complete
// Usage:
//   auto stream = workflow.executeStreaming(engine, params);
//   for (auto& result : stream) {
//       std::cout << result.ruleId << ": " << result.isSuccess() << "\n";
//   }
//   // Backpressure support:
//   stream.pause();  // Temporarily halt rule execution
//   stream.resume(); // Continue
class StreamingResult {
public:
    using Generator = std::function<std::optional<RuleResult>()>;

    explicit StreamingResult(Generator gen) : generator_(std::move(gen)) {}

    // Backpressure control
    void pause() { paused_ = true; }
    void resume() { paused_ = false; }
    [[nodiscard]] bool isPaused() const { return paused_.load(); }

    class Iterator {
    public:
        Iterator() = default;
        explicit Iterator(StreamingResult* parent) : parent_(parent) {
            advance();
        }

        RuleResult& operator*() { return current_.value(); }
        RuleResult* operator->() { return &current_.value(); }

        Iterator& operator++() {
            advance();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return hasValue_ != other.hasValue_;
        }

    private:
        void advance() {
            if (!parent_) {
                hasValue_ = false;
                return;
            }

            // Wait if paused (backpressure)
            while (parent_->isPaused()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            current_ = parent_->generator_();
            hasValue_ = current_.has_value();
        }

        StreamingResult* parent_ = nullptr;
        std::optional<RuleResult> current_;
        bool hasValue_ = false;
    };

    Iterator begin() { return Iterator(this); }
    Iterator end() { return Iterator(); }

private:
    Generator generator_;
    std::atomic<bool> paused_{false};
};

} // namespace fastrules
