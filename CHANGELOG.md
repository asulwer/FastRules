# Changelog

All notable changes to FastRules will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **BREAKING: `coExecuteWorkflow` now takes its `Workflow` by value.** It
  previously took `Workflow&` and moved from it, silently leaving the caller's
  workflow empty with nothing at the call site to indicate that. Ownership is
  now explicit — pass `std::move(workflow)`:
  ```cpp
  auto task = coExecuteWorkflow(std::move(workflow), engine, params, 4);
  ```
  Taking it by value (rather than by rvalue reference) also means the coroutine
  frame owns the workflow outright, so it cannot dangle.

### Fixed
- **Compiled Lua references are now tracked per engine.** `Rule` kept a single
  `compiledExpressionRef` shared across every engine, while the "already
  compiled?" guard checked a map that was never populated. Compiling a second
  workflow into an engine that had already compiled another therefore left rules
  holding a reference number belonging to a *different* Lua state, and execution
  silently evaluated the wrong expression — returning a wrong result with no
  error. References are now keyed by a new `LuaEngine::instanceId()` (never
  reused, so a recycled engine address cannot inherit stale entries) and
  invalidated when the engine's state generation or the rule's own
  expression/action text changes. Rules also compile lazily into any engine they
  have not been compiled for, and repeated `compile()` calls no longer leak a
  Lua registry reference each time.
- **`AsyncTask<T>` use-after-free (crash).** `final_suspend()` returned
  `std::suspend_never`, so the coroutine frame destroyed itself at `co_return`;
  `get()` then read a destroyed promise and `~AsyncTask` destroyed the frame a
  second time (observed as a segfault). It now suspends at final suspend, is
  move-only, and propagates exceptions instead of calling `std::terminate`.
- **`RuleVersionManager::snapshotWorkflow` deadlock.** It held `mutex_` and then
  called `snapshotRule()`, which re-locked the same non-recursive mutex —
  throwing `resource deadlock would occur` on MSVC and hanging on
  libstdc++/libc++. Split into a locked helper. The same defect in
  `DbWorkflowRepository::findAll` (re-acquiring a held `std::shared_mutex`, which
  is undefined behaviour) is fixed the same way.
- **Expression validator no longer rejects ordinary identifiers.**
  `findDangerousPatterns` matched bare substrings, so `payload`, `download_count`
  and `package_type` all tripped the `load`/`package` patterns and could not be
  compiled at all. It now matches on identifier boundaries, as
  `SandboxManager::validateCode` already did. `input_validator` additionally
  stops treating `_` as a boundary (which rejected names like `my_run_count`).
- **Out-of-bounds read building the duplicate-rule-ID error.**
  `Rule::validate` built its message with `"literal" + id`, i.e. pointer
  arithmetic on a string literal, producing garbage text and risking a crash for
  larger ids.
- **`Workflow::compileParallel` no longer fails on rules with children.**
  `buildCompilationLevels` counted child rules as in-edges on their parent but
  only ever decremented from top-level rules, so any workflow of 10+ rules
  containing a rule with children threw "Not all rules were compiled". Levels are
  now computed only over rules actually present in the workflow, worker threads
  claim rules atomically (two workers could previously compile the same rule
  concurrently), and levels are separated by a real join barrier.
- **`AsyncWorkflow` shutdown use-after-free.** Member order destroyed the engine
  pool before the thread pool joined its workers, so an in-flight task could use
  a freed `LuaEngine`. The thread pool is now declared last (destroyed first).
- **Unbounded growth of Lua action handlers.** `bindActions` appended a fresh
  copy of every handler on each call — and it is called on every
  `compileAction()`, every `Workflow::compile()` and once per engine clone.
  Handlers now reuse a stable slot per name.
- **`luaL_error` no longer longjmps over live C++ objects** in the predicate and
  action-callback closures (which skipped destructors and, in one case, unwound
  out of a `catch` block). C++ exceptions crossing the Lua C boundary are now
  converted to Lua errors instead of propagating.
- **`LuaValue`s surviving a `resetState()`** no longer unref into a closed Lua
  state; they share a liveness flag with the backend and degrade to nil.
  Metatable method pointers now reference backend-owned storage with a stable
  address instead of the type registry's vector, which re-registration replaces.
- **Sandbox and rule timeouts no longer disable each other.** A `lua_State` has
  one hook slot; applying the sandbox replaced `LuaEngine`'s timeout hook (and
  vice versa). The sandbox hook now chains to, and on removal restores, the hook
  it displaced. `applySandbox` also verifies a cached entry belongs to the state
  in front of it, so a `lua_State` closed without `removeSandbox()` cannot leave
  a new state at the same address silently unsandboxed.
- **Rate limiting configured by rule name now applies.** `Rule::execute` only
  ever looked up `std::to_string(id)`, while `RateLimiter::Config` is keyed by
  `ruleName`; both keys are now checked.
- **Parameter type validation is no longer platform-dependent.** It compared
  against `std::type_info::name()`, which is implementation-defined — "int" on
  MSVC but "i" on libstdc++ — so the check silently did nothing on GCC/Clang.
  It now compares `std::type_index` values.
- **Results of unnamed rules no longer overwrite each other** in `RuleContext`;
  they were all stored under the empty-string key. Added `Rule::resultKey()`.
- **`Workflow::executeParallel` no longer spawns one thread per rule.**
  Concurrency is capped to the engine-pool size, so a large dependency level no
  longer creates hundreds of threads that fail with a pool-acquire timeout.
- **`Workflow::executeStreaming` no longer captures `this`**, so the returned
  generator does not dangle if the workflow is destroyed first.
- **Nested rule timeouts** no longer clear an enclosing rule's deadline, and
  `LuaEngine`'s state mutex is recursive so an action callback may re-enter the
  engine without self-deadlocking.
- **Repository durability.** JSON and XML rule stores now write via a
  unique temporary file plus atomic rename and report failures, instead of
  silently discarding pending changes when the write failed;
  `XmlRuleRepository` gained the destructor flush its JSON counterpart already
  had (without it, `remove()` was lost), `toString()` now serialises the
  document instead of always returning `""`, and both stores round-trip
  `description` and `cacheDuration`.
- **Hardening of untrusted input.** `JsonLoader` bounds `childRules` nesting
  (unbounded recursion could overflow the stack), reports non-numeric string ids
  as `RuleException` instead of a bare `std::invalid_argument`, and uses a
  single atomic workflow-id counter instead of two independent statics that
  handed out colliding ids. DB rows tolerate NULL columns rather than throwing,
  and `RuleVersion::createdAt` is no longer dropped when reading versions back.
- **Smaller fixes.** `AotCompiler` guards a null `lua_tostring`;
  `MemoryPool`/`VectorPool` no longer wrap their unsigned allocation counter to
  `SIZE_MAX`; `TimeoutExecutor` joins its worker on the success path instead of
  leaking a detached thread per call, and clamps a negative soft timeout;
  `WorkStealingThreadPool::enqueue` rejects work after shutdown rather than
  returning a future that can never complete; the C API guards its global
  registries with a mutex and parses an empty value as an empty string rather
  than `0`; `fastrules::logger()` no longer caches the default logger, so a
  later `spdlog::set_default_logger()` takes effect; `std::tolower`/`isalnum`
  are called with `unsigned char` (UB for non-ASCII input); and the unused
  `WorkStealingQueue` carries a `static_assert` plus documentation of why it is
  not production ready.
- **Deterministic rule execution order.** `Workflow::buildDependencyLevels`
  collected ready rules in `std::unordered_map` iteration order and ordered them
  with a non-stable `std::sort`, so independent rules of equal priority could run
  (and report results) in an implementation-defined order that differed between
  standard libraries. It now collects rules in the workflow's insertion order and
  uses `std::stable_sort`, giving a stable, portable order across platforms.
- **Non-Windows builds of the persistence extensions.** The JSON, XML, and DB
  extension sources used backslash include paths (e.g. `<fastrules\…>`), which
  fail on toolchains that treat the backslash literally; switched to forward
  slashes.
- **Missing `<mutex>` include in `rule_context.cpp`**, which uses
  `std::unique_lock` but only included `<shared_mutex>` (it compiled only where
  `<mutex>` was pulled in transitively).
- **`RuleResult` timing test** assigned `high_resolution_clock` time points to
  `steady_clock` fields; it now uses `steady_clock` to match the result type.
- **Sandbox resource limits now enforced.** `setMemoryLimit` installs a capping
  `lua_Alloc`, and `setInstructionLimit` installs a count hook that raises a Lua
  error when the budget is exceeded (both were previously no-op placeholders).
  `SandboxManager` is now thread-safe and `getSandboxManager()` uses a
  thread-safe singleton; `validateCode` uses word-boundary matching to stop
  false positives (e.g. `payload`, `reload`) while still rejecting dangerous
  calls. `coroutine` is no longer restricted by default since the engine exposes
  coroutine features.
- **C API no longer leaks per-engine state.** `fastrules_engine_destroy` now
  clears the global type registries keyed by the engine pointer, preventing
  leaks and stale state on pointer reuse.
- **`EnginePool::tryPop` honors its timeout.** It now waits for an engine to be
  returned instead of returning `nullptr` immediately when the pool is empty.
- **Per-rule result cache is bounded** (max 1024 entries) with expiry-based
  eviction, preventing unbounded growth under highly variable parameters.
- **Lua parameter marshalling** handles `int64_t`, `long`, `long long`, the
  unsigned integer types, and `float` instead of silently converting them to nil.
- **Backend `reset()` clears action handlers**, so handler IDs restart from 0
  and stale handlers are not retained.
- **Work-stealing pool drains queued tasks on shutdown** (previously dropped,
  leaving their futures unsatisfied), clamps a 0-thread pool to 1 worker, and
  drops the unused `idleWorkers_` counter.
- **Version reporting unified** to 0.2.0 across CMake and `fastrules_get_version`.
- **`<coroutine>` is now included unconditionally in `rule_result.hpp`.** It was
  previously guarded by `#ifdef __cpp_lib_coroutine`, but `AsyncTask` uses
  `std::coroutine_handle`/`std::suspend_never` unconditionally. Because the
  feature-test macro is only defined once `<coroutine>` (or `<version>`) is
  included, the guard made the header rely on include order and could fail to
  compile in isolation.
- **C API `fastrules_add_typed_param` / `fastrules_add_object_param` report
  allocation failures.** They now return `FASTRULES_ERROR_MEMORY` when the output
  buffer allocation fails (previously returned `FASTRULES_OK` with a null/unset
  out-parameter) and initialize `*out_params` to `NULL` on entry so callers never
  observe an uninitialized pointer on the error paths.
- **`getSecurityConfig()` uses a thread-safe function-local static** instead of a
  lazily `new`-ed global pointer, removing a data race on first use and a
  process-exit leak.

### Documentation
- **Corrected the API reference.** Examples that used string rule/workflow IDs
  now use `int` (the actual `Rule::Id` / `Workflow::id` type), the LuaEngine
  description no longer claims a removable "sol2 or LuaBridge3" backend choice,
  and the API Reference section now lists its child pages in the navigation.
- **Fixed stale version references** (`0.1.0` → `0.2.0`) in the install
  instructions and `docs/Doxyfile`.
- **Fixed the execution-tracing snippet** in the performance guide (correct
  `ExecutionTracer(workflow.id)` constructor and `ruleName` field).
- **New documentation pages and sections**: a [Predicate Reference](docs/predicates.md)
  (Lua built-ins + `Rule::` factories), an [Observability guide](docs/observability.md)
  (execution tracing + performance counters), documented rate limiting (replacing
  the "Planned" stub) in the security guide, and `EnginePool` / `MemoryManager`
  pooling in the performance guide.
- **Added a `Docs Check` CI workflow** that verifies documented versions match
  `CMakeLists.txt` and guards against re-introducing known stale snippets.
- **Refocused the README on getting started.** The README is now a concise
  showcase (why FastRules, build, your first rule, optional file-based rules, and
  a documentation map) instead of duplicating the full install matrices,
  extension architecture, and example tables. The complete reference lives in
  `docs/`, where `docs/index.md` now carries a full documentation map linking
  every page. Also corrected stale `0.1.0` version references in the README.

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
