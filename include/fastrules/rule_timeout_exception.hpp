/**
 * @file rule_timeout_exception.hpp
 * @brief Shared timeout exception used by rule execution and the timeout executor.
 */

#pragma once

#include <stdexcept>
#include <string>

namespace fastrules {

/**
 * @brief Exception thrown when rule execution exceeds its timeout.
 *
 * Used by both the core rule execution path and the standalone TimeoutExecutor.
 */
class RuleTimeoutException : public std::runtime_error {
public:
    explicit RuleTimeoutException(const std::string& message)
        : std::runtime_error(message) {}
};

} // namespace fastrules
