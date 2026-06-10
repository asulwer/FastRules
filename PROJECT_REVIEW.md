# FastRules Project Review

**Date:** 2026-06-10  
**Commit:** 75fdc55

## 🔴 Critical Issues (Fix Immediately)

| # | Issue | Location | Details | Fix |
|---|-------|----------|---------|-----|
| 1 | **Undefined behavior: nullptr dereference in member offset** | `include/fastrules/lua_engine.hpp:58` | `reinterpret_cast<size_t>(&(((T*)nullptr)->*member))` is UB. Compilers optimize `nullptr` derefs as unreachable. | Use `offsetof` macro or create a real temporary: `T t{}; return reinterpret_cast<size_t>(&(t.*member));` |
| 2 | **Dangling references in async execution** | `src/async_workflow.cpp:~120` | Lambda captures `&engine`, `&context`, `&parameters` by reference, then enqueues to thread pool. If caller returns before tasks finish, references dangle. | Capture by value or wrap in `shared_ptr`. Copy `parameters` vector and pass engine clone to each task. |
| 3 | **Thread pool destructor not exception-safe** | `src/async_workflow.cpp:~45` | If a worker throws during `join()`, destructor propagates exception = `std::terminate`. | Wrap `join()` in try/catch or use `std::jthread` (C++20). |
| 4 | **`std::regex` in hot path** | `src/lua_engine.cpp:81-160` | `extractParameterNames` creates and runs 3 `std::regex` objects per compilation. `std::regex` is notoriously slow (often 100x slower than hand-rolled). | Replace with a simple state-machine parser or `std::string_view` scan. Cache the keyword sets as `static constexpr`. |

---

## 🟡 Warnings (Should Fix)

| # | Issue | Location | Details | Fix |
|---|-------|----------|---------|-----|
| 5 | **Hard public dependency on spdlog** | `include/fastrules/lua_engine.hpp:18` | `<spdlog/spdlog.h>` in public header forces all consumers to have spdlog. Breaks header-only or minimal builds. | Forward-declare `spdlog::logger` or use PIMPL for logger. |
| 6 | **Unnecessary heavy include in `workflow.hpp`** | `include/fastrules/workflow.hpp:11` | Includes `lua_engine.hpp` "for unique_ptr destructor" but destructor is in .cpp. Adds huge transitive deps. | Remove include; use forward declaration. |
| 7 | **Heap-allocated mutex** | `include/fastrules/workflow.hpp:101` | `std::unique_ptr<std::mutex> poolMutex_` — pointless heap allocation. | Use `std::mutex poolMutex_` directly. |
| 8 | **Incomplete Lua C API forward declarations** | `src/lua_engine.cpp:19-28` | Re-declares `lua_sethook` and `lua_Debug` instead of including `<lua.hpp>` or `<lauxlib.h>`. Risk of ABI mismatch on Lua updates. | Include proper Lua headers. |
| 9 | **Silent exception swallowing** | `src/lua_engine.cpp:~155` | `anyToLuaValue` catches `(...)` and returns nil without logging. Hides real errors. | Log the exception type/value before falling back. |
| 10 | **`reinterpret_cast` for type punning** | `src/lua_backend_luabridge.cpp:775-781` | `*reinterpret_cast<int*>(ptr + offset)` assumes alignment and strict aliasing compliance. UB if types don't match. | Use `std::memcpy` for safe type punning, or verify alignment. |
| 11 | **Builder copies rule vector** | `include/fastrules/rule.hpp:~260` | `dependsOn(Id, vector<reference_wrapper<const Rule>>)` takes vector by value and copies it for validation. | Take `const vector&` or `span`. |
| 12 | **CI benchmark job references missing file** | `.github/workflows/ci.yml:89` | `git show main:benchmarks\baseline.txt` but `benchmarks/` directory doesn't exist. | Create baseline or disable benchmark job. |
| 13 | **Ubuntu CI doesn't test extensions** | `.github/workflows/ci.yml:48` | Only installs `liblua5.4-dev catch2 nlohmann-json3-dev`. Missing `soci`, `pugixml`, so DB/XML extensions untested on Linux. | Add extension dependencies to apt install. |
| 14 | **CMake config claims no dependencies** | `cmake/fastrules-config.cmake.in:6` | "# No external dependencies needed" — false. Lua and spdlog are required. | Update to `find_dependency(lua)` / `find_dependency(spdlog)`. |
| 15 | **Conan exports unnecessary files** | `conanfile.py:32` | Exports `docs/`, `data/`, `scripts/` — bloats the package. | Trim to only build-required files. |

---

## 🟢 Notes (Nice to Have)

| # | Issue | Location | Details | Fix |
|---|-------|----------|---------|-----|
| 16 | **Double `public:` in `Rule`** | `include/fastrules/rule.hpp` | Two `public:` sections — confusing. | Consolidate or organize with clear sections. |
| 17 | **No clang-tidy in CI** | `.github/workflows/` | `.clang-tidy` exists but never run in CI. | Add a lint job. |
| 18 | **Missing CODECOV_TOKEN** | `.github/workflows/coverage.yml` | `codecov/codecov-action@v4` may fail without token on private repos. | Add `token: ${{ secrets.CODECOV_TOKEN }}`. |
| 19 | **No release workflow** | `.github/workflows/` | No automated GitHub releases or tag-based builds. | Add `release.yml`. |
| 20 | **No Linux/macOS CMake preset** | `CMakePresets.json` | Only Windows presets. | Add `linux-default` and `macos-default` presets. |
| 21 | **CONTRIBUTING.md wrong test path** | `CONTRIBUTING.md:28` | `./build/tests/fastrules_tests` — actual path is `./build/Release/fastrules_tests.exe` on Windows. | Fix path or make platform-agnostic. |
| 22 | **vcpkg.json lacks version constraints** | `vcpkg.json` | `"lua"` without version could resolve to incompatible versions. | Add version constraints (e.g., `"lua"` with `version>=`). |
| 23 | **`test_package/` never exercised in CI** | `test_package/` | Exists for Conan validation but CI doesn't build it. | Add to CI or remove if unused. |
| 24 | **No issue/PR templates** | `.github/` | Missing `ISSUE_TEMPLATE/`, `PULL_REQUEST_TEMPLATE.md`. | Add templates. |

---

## Summary by Area

| Area | Critical | Warnings | Notes |
|------|----------|----------|-------|
| Code Safety | 4 | 6 | 0 |
| API Design | 0 | 3 | 1 |
| CI/CD | 0 | 2 | 5 |
| Packaging | 0 | 2 | 1 |
| Docs/Repo | 0 | 0 | 5 |

**Total:** 4 critical, 8 warnings, 12 notes

---

## Priority Actions

1. **Fix #1 (nullptr UB)** — Compile-time undefined behavior that may cause incorrect member offset calculations
2. **Fix #2 (dangling refs)** — Memory safety bug in async execution, use-after-free risk
3. **Fix #3 (thread pool exception safety)** — Potential `std::terminate` on cleanup
4. **Fix #4 (regex performance)** — Orders of magnitude slowdown in expression compilation
5. **Fix #5-#7 (API hygiene)** — Clean up public headers for better encapsulation
6. **Fix #12-#13 (CI gaps)** — Ensure extensions actually get tested, benchmarks don't fail on PRs