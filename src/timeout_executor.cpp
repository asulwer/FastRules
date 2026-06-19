#include "fastrules/timeout_executor.hpp"
#include "fastrules/logger.hpp"

#include <signal.h>

namespace fastrules {

RuleTimeoutException::RuleTimeoutException(const std::string& message) 
    : std::runtime_error("Timeout: " + message) {}

// TimeoutExecutor implementation is header-only since it's templated

} // namespace fastrules