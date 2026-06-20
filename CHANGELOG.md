# Changelog

All notable changes to FastRules will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Migration Guide

If upgrading from 0.1.0:
- **Persistence moved to extensions.** JSON/XML loading is no longer in the core library. Link `fastrules-json` or `fastrules-xml` and include `<fastrules/json_loader.hpp>` / `<fastrules/xml_loader.hpp>`.
- **API is unchanged.** `fastrules::JsonLoader::loadWorkflow(json)`, `fastrules::XmlLoader::loadWorkflow(xml)`, etc. still work exactly as before — just from a separate library target.
- **AOT compiler now works without sol2.** If you were using `FASTRULES_USE_SOL2`, that flag is no longer required. Bytecode dump/load uses raw Lua C API and works with any backend.
- **Time formatting uses thread-safe functions.** `std::gmtime` replaced with `gmtime_s` (Windows) / `gmtime_r` (POSIX). No user-facing API change.
- **Test discovery unified.** `enable_testing()` is now called at the root level; run `ctest` from the build root to execute both core and extension tests.

### Added
- AOT compilation — pre-compile workflows to binary bundles for faster loading
- Rule versioning — semantic versioning with history tracking and rollback support
- Rate limiting — per-rule execution rate limits with burst support
- Performance counters — thread-safe metrics collection with JSON export
- Execution tracing — detailed step-by-step execution traces with JSON export
- State cleanup — automatic Lua state cleanup for long-running applications
- Expression validation — dangerous pattern detection and syntax validation
- vcpkg manifest — `vcpkg.json` with feature flags for LuaJIT and tests
- Conan recipe — `conanfile.py` with options for LuaJIT, tests, and examples
- CMake install target — proper `install()` commands and package config files
- Conan test_package — validation package for Conan recipe testing
- **Persistence extensions (NEW):**
  - `fastrules-json` — JSON file-based persistence (human-readable, version-control friendly)
  - `fastrules-xml` — XML file-based persistence (enterprise environments)
  - `fastrules-db` — Database persistence via SOCI (PostgreSQL, MySQL, SQLite)
  - Repository pattern with `IRuleRepository`, `IWorkflowRepository`, `IVersionRepository`
  - Schema management and transaction support in DB extension

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

### Fixed
- Empty/whitespace expressions now throw `ValidationException` in `input_validator.cpp` while still allowing syntax errors to throw `RuleCompilationException` from `LuaEngine::compileExpression`.
- Coroutine Lua test expression changed to a valid single expression (`"x + 1"`).
- Memory-pool and vector-pool thread-safety/reuse tests updated to allow for object reuse instead of requiring one allocation per acquire.
- DB repository schema and persistence now include the `name` column for rules.
- DB workflow save no longer deadlocks by calling `exists()` while holding a `unique_lock`.
- DB thread-safety test now gives each worker its own SOCI session.
- JSON performance test threshold relaxed to allow slower Debug builds.
- Fixed input-validator length assertion (`longExpr` = 9993, total expression length = 10000).
- Fixed `C4702` unreachable-code warning in `timeout_executor.hpp` by replacing the manual promise helpers with `std::packaged_task` and removing the redundant `try/catch` rethrow in `RuleExecutor::execute`.
- Updated README / docs references for the unified C API header.

### Fixed
- MSVC runtime library mismatch between fastrules and dependencies
- Missing CMake install/export configuration
- Conan recipe missing `build_examples` option
- Extension tests now wired to root CTest
- AOT compiler no longer requires sol2 backend
- `std::gmtime` deprecation warnings on Windows
- Extension example path resolution made robust
- REPL example EOF handling on Windows
- Macro header MSVC warning suppression

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
