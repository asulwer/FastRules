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
- Updated README with accurate requirements and installation options
- Fixed MSVC runtime library linkage (LNK4098 warnings)
- CI workflow now generates code coverage reports
- CI workflow now produces vcpkg and Conan artifacts
- vcpkg.json now includes version override for Lua 5.4.6

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
