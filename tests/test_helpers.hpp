// test_helpers.hpp
// Common test utilities for FastRules test suite.

#pragma once

#include <fastrules/lua_engine.hpp>
#include <spdlog/logger.h>
#include <memory>

// Forward declaration from test_main.cpp
std::shared_ptr<spdlog::logger> getTestLogger();

namespace fastrules {

// Helper: create a LuaEngine with test logging configured.
// Use this instead of raw LuaEngine() in tests to get debug output.
inline LuaEngine makeTestEngine() {
    LuaEngine engine;
    engine.setLogger(getTestLogger());
    return engine;
}

} // namespace fastrules
