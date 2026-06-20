// test_main.cpp
// doctest main entry point for DB extension tests

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef _WIN32
#include <windows.h>
#include <string>

// SOCI loads its SQLite backend plugin at runtime. Make sure the loader looks
// in the directory that contains this test executable so the backend DLLs are
// found regardless of how the test is launched.
struct SociBackendPathSetup {
    SociBackendPathSetup() {
        wchar_t path[MAX_PATH] = {0};
        if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return;
        std::wstring ws(path);
        auto pos = ws.find_last_of(L"\\/");
        if (pos == std::wstring::npos) return;
        std::wstring dir = ws.substr(0, pos);
        // Keep any existing search path and prepend the executable directory.
        wchar_t existing[32768] = {0};
        GetEnvironmentVariableW(L"SOCI_BACKENDS_PATH", existing, 32768);
        std::wstring value = dir;
        if (existing[0] != L'\0') {
            value += L";" + std::wstring(existing);
        }
        SetEnvironmentVariableW(L"SOCI_BACKENDS_PATH", value.c_str());
    }
} g_sociBackendPathSetup;
#endif

