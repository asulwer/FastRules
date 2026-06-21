# Changelog

All notable changes to FastRules will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- Removed CTest integration. Tests are now run as plain doctest executables.
  - Removed `enable_testing()` and `add_test()` registrations from root and extension test CMakeLists.
  - Extension test targets now copy `fastrules.dll` next to the executable at build time so they can be run directly.
  - Updated `README.md` to show direct execution instead of `ctest`.


## [0.2.0] - 2026-06-20

### Migration Guide

If upgrading from 0.1.0:
- **Persistence moved to extensions.** JSON/XML loading is no longer in the core library. Link `fastrules-json` or `fastrules-xml` and include `<fastrules/json_loader.hpp>` / `<fastrules/xml_loader.hpp>`.
- **API is unchanged.** `fastrules::JsonLoader::loadWorkflow(json)`, `fastrules::XmlLoader::loadWorkflow(xml)`, etc. still work exactly as before — just from a separate library target.
- **AOT compiler now works without sol2.** If you were using `FASTRULES_USE_SOL2`, that flag is no longer required. Bytecode dump/load uses raw Lua C API and works with any backend.
- **Time formatting uses thread-safe functions.** `std::gmtime` replaced with `gmtime_s` (Windows) / `gmtime_r` (POSIX). No user-facing API change.
- **Test discovery unified.** `enable_testing()` is now called at the root level; run `ctest` from the build root to execute both core and extension tests.

### Added
- Core stress-test suite (standalone executable, not tied to CTest or doctest):
  - `FASTRULES_BUILD_STRESS_TESTS` CMake option (default `OFF`) to mirror the existing test/example flags.
  - `fastrules-stress-core` executable covering compile throughput, execute throughput, parallel execution, engine-pool exhaustion, concurrent compile+execute, auto-reset stress, large workflows, deep child-rule chains, action throughput, timeout-executor storm, `executeAsync` backlog, coroutine churn, type-registration churn, parameter bloat, exception-path stress, engine clone pressure, and mixed-workload soak.
  - Controlled via command-line knobs: `--duration`, `--iterations`, `--threads`, `--rules`, and `--parameters`.
- Fixed coroutine memory leak: `LuaBridge3Backend::createCoroutine` left every new Lua thread referenced on the main state's stack, and `closeCoroutine` did not release that reference. Now each coroutine is stored via a registry reference and properly unreferenced on close/reset. Stress-test `coroutine churn` memory dropped from ~600 MB to ~50 KB as a result.

- Fixed duplicated `RuleTimeoutException` definition by extracting it into `include/fastrules/rule_timeout_exception.hpp` and including it from both `rule.hpp` and `timeout_executor.hpp`.

- AOT compilation — pre-compile workflows to binary bundles for faster loading.
- Rule versioning — semantic versioning with history tracking and rollback support.
- Rate limiting — per-rule execution rate limits with burst support.
- Performance counters — thread-safe metrics collection with JSON export.
- Execution tracing — detailed step-by-step execution traces with JSON export.
- State cleanup — automatic Lua state cleanup for long-running applications.
- Expression validation — dangerous pattern detection and syntax validation.
- Security hardening — memory pooling, timeout enforcement, input validation, and sandboxing.
- Work-stealing thread pool and C++20 coroutine support with full integration and testing.
- vcpkg manifest — `vcpkg.json` with feature flags for LuaJIT and tests.
- Conan recipe — `conanfile.py` with options for LuaJIT, tests, and examples.
- CMake install target — proper `install()` commands and package config files.
- Conan `test_package` — validation package for Conan recipe testing.
- **Persistence extensions (NEW):**
  - `fastrules-json` — JSON file-based persistence (human-readable, version-control friendly).
  - `fastrules-xml` — XML file-based persistence (enterprise environments).
  - `fastrules-db` — Database persistence via SOCI (PostgreSQL, MySQL, SQLite).
  - Repository pattern with `IRuleRepository`, `IWorkflowRepository`, `IVersionRepository`.
  - Schema management and transaction support in DB extension.
- Comprehensive test files for all core components and extension repositories.

### Changed
- Refactored CMake build system:
  - Removed `FASTRULES_BUILD_C_API` option; the C API is always exported from the core library via `fastrules.h`.
  - Removed duplicate compiler warning flags and duplicate `aot_compiler.cpp` source listing.
  - `FASTRULES_BUILD_EXTENSIONS=ON` now builds all three extensions (json, xml, db).
  - Generated examples and gathered tests via loops / `file(GLOB)` instead of one-by-one boilerplate.
  - Extension CMakeLists now use `find_package` first with `FetchContent` fallback and no longer hard-code dependency source paths.
  - DB test CMake now copies only matching-config vcpkg DLLs, creates SOCI backend aliases, and sets the backend search path via the test runner environment.
- Refactored `build.ps1`:
  - Supports `-Configuration {Debug|Release|Both}` (default `Debug`).
  - No longer forces shared libraries, kills processes, manually copies DLLs, or renames SOCI backends.
  - Auto-detects vcpkg and only passes the toolchain when not already cached.
  - Runs `ctest` with `--timeout 120` to prevent hangs.
- Memory pool accounting now treats `allocatedCount_` as live objects and decrements it when objects are destroyed or `clear()` discards pooled objects.
- `LuaEngine::buildParamPairs` no longer overwrites missing parameters with `nil`, preserving globals set via `setGlobal`.
- `DbConnectionFactory` uses `soci::factory_sqlite3()` directly for the SQLite backend, avoiding SOCI dynamic backend-loader issues on Windows.
- `compileExpression` now returns `std::nullopt` only for empty/whitespace input; syntax errors still throw `RuleCompilationException`.
- `std::gmtime` replaced with `gmtime_s` (Windows) / `gmtime_r` (POSIX) in JSON serialization for thread safety.

### Fixed
- Core compilation errors across the library, examples, and extensions.
- RuleContext copy constructor implementation.
- Duplicate rule/workflow repository code consolidated.
- Thread-safety issues in core and DB extension tests.
- Timeout executor reliability issues and unreachable-code warnings.
- Sandbox violations and unsafe runtime behavior.
- C API export macro redefinition (`FASTRULES_C_API`) now exported only through `fastrules.h` / `fastrules_export.hpp`.
- Empty/whitespace expressions now throw `ValidationException` in `input_validator.cpp` while still allowing syntax errors to throw `RuleCompilationException` from `LuaEngine::compileExpression`.
- Coroutine Lua test expression changed to a valid single expression (`"x + 1"`).
- Memory-pool and vector-pool thread-safety/reuse tests updated to allow for object reuse instead of requiring one allocation per acquire.
- DB repository schema and persistence now include the `name` column for rules.
- DB workflow save no longer deadlocks by calling `exists()` while holding a `unique_lock`.
- DB thread-safety test now gives each worker its own SOCI session.
- JSON performance test threshold relaxed to allow slower Debug builds.
- Input-validator length assertion fixed (`longExpr` = 9993, total expression length = 10000).
- `C4702` unreachable-code warning in `timeout_executor.hpp` fixed by replacing manual promise helpers with `std::packaged_task` and removing the redundant `try/catch` rethrow in `RuleExecutor::execute`.
- Coroutine `AsyncWorkflowTask` no longer self-destructs its coroutine frame (`final_suspend` now returns `suspend_always`), eliminating the `STATUS_ACCESS_VIOLATION` crash in `coroutine_example`.
- `AsyncWorkflow::executeParallelAsync` now passes a shared `RuleContext` across dependency levels so rules can correctly read results from prior levels.
- `TypeRegistry::registerType` merges descriptors instead of replacing them, so macros like `FASTRULES_REGISTER_METHODS_N` can add methods after `FASTRULES_REGISTER_TYPE_N` has bound fields.
- `LuaEngine::compileExpression`, `compileAction`, and `compileCoroutine` now bind registered types/actions before compiling, fixing direct `Rule::compile()` usage in `no_globals_example`.
- `LuaEngine::compileExpression`, `compileAction`, and `compileCoroutine` now lock `luaStateMutex_` while touching the Lua backend, preventing concurrent compile/execute threads from corrupting the Lua state.
- The `refToBackendId_` registration is now performed while still holding `registryMutex_`, removing a race where concurrent execution could see a ref before its backend ID was set.
- `Workflow::executeStreaming` no longer uses `static thread_local` generator state; each `StreamingResult` owns its own `RuleContext` and index, preventing cross-generator corruption and memory leaks.
- DB example CMake now copies `fmt.dll`/`fmtd.dll` so the DB example no longer fails with `STATUS_DLL_NOT_FOUND`.
- Extension example/test runtime failures (`STATUS_DLL_NOT_FOUND`) resolved by copying only the matching build configuration's vcpkg DLLs to each output directory, avoiding Debug/Release runtime mixups.
- `TimeoutExecutor::executeWithTimeout` no longer detaches a stack-allocated `std::thread`; the task is heap-owned by the detached worker, removing the use-after-free risk on the local worker handle.
- `Workflow::executeAsync` no longer captures the caller-supplied `LuaEngine` by reference. It runs on a pre-compiled engine clone from the workflow pool, so the returned future does not depend on caller objects.
- `WorkStealingThreadPool::enqueue` uses a non-static distribution sized to the current pool, so a thread submitting to multiple pools cannot produce out-of-range indices.
- `LuaEngine` auto-reset now triggers *before* compiling a new expression/action/coroutine. Resetting after compilation was invalidating the reference that had just been returned to the caller.
- `LuaEngine::getMemoryUsageKB()` now takes the Lua-state mutex normally when called without already holding it; `resetState()` uses a private unsafe helper to avoid self-deadlock.
- DB example now uses an absolute database path so SQLite can open `rules.db` regardless of the caller's working directory.
- `coroutine_example` no longer relies on an unsupported global `result` table in actions.
- DB `soci::rowset` loops rewritten with explicit iterators to avoid unreachable-code warnings.
- `std::gmtime` deprecation warnings on Windows.
- Extension example path resolution made robust.
- REPL example EOF handling on Windows.
- Macro header MSVC warning suppression.
- MSVC runtime library mismatch between fastrules and dependencies.
- Missing CMake install/export configuration.
- Conan recipe missing `build_examples` option.
- Extension tests now wired to root CTest.

### Removed
- `todo.md` — no longer needed after resolving all build/runtime errors.
- Temporary cache files and backup files cleaned from the repository.

## [0.1.0] - 2024-06-05

### Added
- Initial release
- Lua expression and action evaluation
- JSON workflow and rule loading
- Workflow validation and compilation
- Sequential and parallel execution
- Streaming results
- Dependency chain support
- C++ type registration in Lua
- Enum registration
- Rule builder pattern
- Workflow builder pattern
- Action callbacks
- Structured logging
- JSON pretty-printing
- 87 unit tests
