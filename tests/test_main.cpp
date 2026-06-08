// test_main.cpp
// Catch2 main with exception logging wrapper to capture MSVC debug assertions.

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <fastrules/logger.hpp>
#include <iostream>
#include <exception>
#include <cstdlib>

// Global logger for test diagnostics
static fastrules::Logger g_testLogger;

static void setupTestLogging() {
    g_testLogger.setHandler([](const fastrules::LogEntry& entry) {
        const char* levelStr = "?";
        switch (entry.level) {
            case fastrules::LogLevel::Trace:   levelStr = "TRACE"; break;
            case fastrules::LogLevel::Debug:   levelStr = "DEBUG"; break;
            case fastrules::LogLevel::Info:    levelStr = "INFO";  break;
            case fastrules::LogLevel::Warning: levelStr = "WARN";  break;
            case fastrules::LogLevel::Error:   levelStr = "ERROR"; break;
            case fastrules::LogLevel::Fatal:   levelStr = "FATAL"; break;
        }
        std::cerr << "[" << levelStr << "] " << entry.message;
        if (!entry.ruleId.empty()) std::cerr << " (rule: " << entry.ruleId << ")";
        std::cerr << "\n";
    });
    g_testLogger.setMinLevel(fastrules::LogLevel::Debug);
}

// Provide a way for tests to access the logger
fastrules::Logger& getTestLogger() {
    static bool initialized = false;
    if (!initialized) {
        setupTestLogging();
        initialized = true;
    }
    return g_testLogger;
}

// Wrapper to catch structured exceptions / debug assertions on Windows
#if defined(_WIN32) && defined(_DEBUG)
#include <windows.h>

static LONG WINAPI vectoredExceptionHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == STATUS_ARRAY_BOUNDS_EXCEEDED ||
        code == STATUS_ACCESS_VIOLATION ||
        code == STATUS_INVALID_HANDLE) {
        getTestLogger().fatal("Structured exception caught: code=" + std::to_string(code));
        std::cerr << "FATAL: Structured exception caught during test execution.\n";
        std::cerr << "Exception code: 0x" << std::hex << code << std::dec << "\n";
        std::abort();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

struct ExceptionHandlerInstaller {
    ExceptionHandlerInstaller() {
        AddVectoredExceptionHandler(1, vectoredExceptionHandler);
    }
} g_exceptionHandlerInstaller;
#endif
