// test_main.cpp
// doctest main entry point

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <fastrules/logger.hpp>
#include <spdlog/sinks/ostream_sink.h>
#include <iostream>

// Global test logger -- configured once per test session
static std::shared_ptr<spdlog::logger> g_testLogger;

// Provide access to test logger for other test files
std::shared_ptr<spdlog::logger> getTestLogger() {
    return g_testLogger;
}

static void setupTestLogging() {
    // Create an ostream sink that writes to std::cerr
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cerr, true);
    g_testLogger = std::make_shared<spdlog::logger>("test_logger", sink);
    g_testLogger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(g_testLogger);
}

// doctest main is provided by DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
// This runs before main
struct TestSetup {
    TestSetup() {
        setupTestLogging();
    }
} g_testSetup;
