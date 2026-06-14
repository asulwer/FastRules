// test_main.cpp
// Catch2 main with exception logging wrapper to capture MSVC debug assertions.

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <fastrules/logger.hpp>
#include <spdlog/sinks/ostream_sink.h>
#include <iostream>
#include <exception>
#include <cstdlib>

// Global test logger -- configured once per test session
static std::shared_ptr<spdlog::logger> g_testLogger;

static void setupTestLogging() {
    // Create an ostream sink that writes to std::cerr
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cerr, true);
    g_testLogger = std::make_shared<spdlog::logger>("test_logger", sink);
    g_testLogger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(g_testLogger);
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
}

// Provide a way for tests to access the logger
std::shared_ptr<spdlog::logger> getTestLogger() {
    static bool initialized = false;
    if (!initialized) {
        setupTestLogging();
        initialized = true;
    }
    return g_testLogger;
}

// ------------------------------------------------------------------------------
// MSVC debug assertion handler -- forwards to spdlog
// ------------------------------------------------------------------------------
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>

static int __cdecl msvcAssertionHandler(int reportType, char* message, int* returnValue) {
    auto log = fastrules::logger();
    if (reportType == _CRT_ASSERT || reportType == _CRT_ERROR) {
        log->critical("MSVC assertion failed: {}", message ? message : "(no message)");
        std::abort();
    }
    if (returnValue) { *returnValue = 0; }
    return TRUE;
}

// Override the default handler on MSVC debug builds
struct MsvcAssertionInstaller {
    MsvcAssertionInstaller() {
        _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, msvcAssertionHandler);
    }
} g_msvcAssertionInstaller;
#endif
