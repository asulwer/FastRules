/**
 * @file test_main.cpp
 * @brief doctest main entry point and test infrastructure
 * 
 * This file provides:
 * - doctest main() entry point via DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
 * - Global test logger configuration
 * - Test setup/teardown infrastructure
 * 
 * Test Logger:
 * - Configured to write to std::cerr via ostream_sink
 * - Debug level logging enabled
 * - Shared across all test files via getTestLogger()
 * 
 * doctest Framework:
 * - Single-header testing framework
 * - Test cases defined with TEST_CASE() macros in other files
 * - Assertions: CHECK(), REQUIRE(), CHECK_EQ(), etc.
 * 
 * Global Test Setup:
 * - TestSetup struct runs before main()
 * - Configures logging infrastructure once
 * 
 * Usage in other test files:
 * @code
 * #include <doctest/doctest.h>
 * 
 * TEST_CASE("my test case") {
 *     CHECK(something == true);
 * }
 * @endcode
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <fastrules/logger.hpp>
#include <spdlog/sinks/ostream_sink.h>
#include <iostream>

/// Global test logger -- configured once per test session
static std::shared_ptr<spdlog::logger> g_testLogger;

/**
 * @brief Get the configured test logger
 * @return Shared pointer to the test logger
 */
std::shared_ptr<spdlog::logger> getTestLogger() {
    return g_testLogger;
}

/**
 * @brief Configure test logging infrastructure
 * 
 * Creates an ostream sink writing to std::cerr
 * with debug level verbosity.
 */
static void setupTestLogging() {
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cerr, true);
    g_testLogger = std::make_shared<spdlog::logger>("test_logger", sink);
    g_testLogger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(g_testLogger);
}

/**
 * @brief Global test setup (runs before main)
 * 
 * Uses the static initialization order guarantee to
 * configure logging before any tests run.
 */
struct TestSetup {
    TestSetup() {
        setupTestLogging();
    }
} g_testSetup;  /// Global instance triggers setup
