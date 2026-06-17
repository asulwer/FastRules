---
layout: default
title: Changelog
nav_order: 18
---

# Changelog

All notable changes to FastRules are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Coming soon...

## [0.2.0] - 2025-06-17

### Added
- **C API**: FFI bindings for Python, C#, and other languages
  - `fastrules.h` header with C-compatible functions
  - Memory management functions (`fastrules_free`, etc.)
  - Full workflow lifecycle support
  - JSON result export from C

- **AsyncWorkflow**: C++20 coroutine support
  - `coExecuteRule()` for single rule async execution
  - `coExecuteWorkflow()` for full workflow async
  - `AsyncWorkflow` class for thread pool management
  - `AsyncRulePromise` and `AsyncWorkflowTask` types

- **Engine Pool**: Manual engine lifecycle management
  - `EnginePool` class for acquiring/releasing engines
  - `acquireEngine()` and `releaseEngine()` methods
  - Timeout support for pool acquisition

- **Rule Result Caching**: Automatic result caching with TTL
  - `Rule::cacheDuration` field
  - Cache key generation from parameters
  - Automatic cache invalidation

- **Rate Limiting**: Per-rule rate limiting
  - `RateLimiter` singleton
  - Sliding window algorithm
  - Configurable per-second and per-minute limits

- **Expression Validation**: Security and syntax checking
  - `ExpressionValidator` class
  - Dangerous pattern detection (os.execute, io.open, etc.)
  - Bracket matching and syntax validation

### Changed
- **LuaEngine API**: `compile()` renamed to `compileExpression()`
  - Returns `std::optional<int>` instead of `int`
  - Better error handling with optional

- **Rule Factory**: Now uses builder pattern
  - `Rule::create(id, expr, active)` returns builder
  - Chain `.withAction()`, `.withPriority()`, etc.
  - Call `.build()` to get `std::shared_ptr<Rule>`

- **Workflow Execution**: Now requires LuaEngine parameter
  - `execute(params)` → `execute(engine, params)`
  - Consistent with other engine-dependent operations

### Deprecated
- `Rule::dependsOnRuleId` → use `Rule::dependsOnRuleName`
- `LuaEngine::compile()` → use `LuaEngine::compileExpression()`
- `Workflow::execute(params)` → use `Workflow::execute(engine, params)`

### Fixed
- Memory leak in Lua callback handling
- Race condition in parallel execution engine pool
- Stack overflow with deeply nested rule dependencies

## [0.1.0] - 2025-05-15

### Added
- **Core Library**: Initial release
  - `Rule` class for single condition-action rules
  - `Workflow` class for rule orchestration
  - `LuaEngine` for Lua expression execution
  - `RuleContext` for passing data between rules
  - `RuleResult` for execution results

- **Type Registration**: C++ types in Lua
  - `engine.registerType<T>()` for structs
  - Automatic field access via `offsetof`
  - Support for int, double, string, bool fields

- **Action Callbacks**: C++ functions callable from Lua
  - `engine.registerAction()` for registration
  - Type-safe parameter passing with `std::any`

- **Parallel Execution**: Multi-threaded rule execution
  - `Workflow::executeParallel()` method
  - Dependency-level parallelism
  - Automatic engine cloning for thread safety

- **Streaming Results**: Generator-style result iteration
  - `Workflow::executeStreaming()` method
  - Coroutine-based lazy evaluation

- **Execution Tracing**: Debug and performance monitoring
  - `ExecutionTracer` class
  - Step-by-step execution recording
  - JSON export for analysis

- **Performance Counters**: Global metrics tracking
  - Execution counts (success, fail, timeout, etc.)
  - Average execution time
  - JSON export

- **Timeout Support**: Per-rule execution timeouts
  - `Rule::timeout` field
  - `RuleTimeoutException` for exceeded timeouts

- **Security Sandboxing**: Dangerous function filtering
  - Blocked: os.*, io.*, debug.*, package.*
  - Pattern detection in expressions
  - Configurable expression length limits

- **Logging Support**: Structured logging with spdlog
  - `engine.setLogger()` for custom loggers
  - Debug, info, warn, error levels

- **Extensions Framework**: Optional add-ons
  - JSON extension (nlohmann/json)
  - XML extension (pugixml)
  - Database extension (SOCI)

- **AOT Compilation**: Pre-compile to bytecode
  - `AotCompiler` class
  - Binary bundle format
  - Hex string encoding

- **Rule Versioning**: Version history and rollback
  - `RuleVersionManager` class
  - Snapshot and restore functionality
  - Diff between versions

### Known Issues
- LuaJIT support incomplete (hooks don't work)
- Windows: Limited Unicode support in expressions
- macOS: Requires Xcode 15+ for C++23 features

## [0.0.9] - 2025-04-01

### Added
- Initial alpha release
- Basic rule evaluation with Lua
- Sequential workflow execution
- JSON loading support

## Migration Guides

See [Migration Guide](migration.md) for detailed upgrade instructions.

## Support

- **Bugs:** [GitHub Issues](https://github.com/asulwer/FastRules/issues)
- **Questions:** [GitHub Discussions](https://github.com/asulwer/FastRules/discussions)
- **Security:** Email maintainers directly

---

[Unreleased]: https://github.com/asulwer/FastRules/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/asulwer/FastRules/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/asulwer/FastRules/compare/v0.0.9...v0.1.0
[0.0.9]: https://github.com/asulwer/FastRules/releases/tag/v0.0.9
