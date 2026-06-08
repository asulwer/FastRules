#include <fastrules/logger.hpp>

namespace fastrules {

static Logger g_globalLogger;

Logger& globalLogger() noexcept {
    return g_globalLogger;
}

void setGlobalLogger(Logger::Handler handler) {
    g_globalLogger.setHandler(std::move(handler));
}

void clearGlobalLogger() {
    g_globalLogger.setHandler(nullptr);
}

} // namespace fastrules
