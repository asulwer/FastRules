# CLAUDE.md

Guidance for AI assistants (Claude Code and others) working in this repository.

## What FastRules Is

FastRules is a **high-performance C++23 business rules engine** that evaluates
rules expressed as **Lua expressions and actions**. It is a C++ rewrite of
[RoslynRules](https://github.com/asulwer/roslynrules), swapping Roslyn/C# IL
compilation for Lua → bytecode (≈1ms compile, ≈2MB memory, self-contained).

The design splits into a **dependency-light core** plus **optional persistence
extensions** (JSON, XML, DB). Core has zero JSON/XML/DB dependencies — pull in
only the extension you need.

## Repository Layout

```
include/fastrules/   Public headers (the API surface; heavily Doxygen-commented)
include/lua.hpp      Wrapper so LuaBridge3 finds Lua (PUC-Rio Lua ships no lua.hpp)
src/                 Core library implementation (.cpp for each subsystem)
tests/               Core test suite (doctest), one test_*.cpp per subsystem
examples/            ~20 runnable examples + csharp_example/ python_example/ (FFI via C API)
stress/              Stress tests (FASTRULES_BUILD_STRESS_TESTS=ON)
extensions/          Optional persistence libraries, each its own CMake target:
  ├── json/          fastrules-json  (nlohmann/json)
  ├── xml/           fastrules-xml   (pugixml)
  ├── db/            fastrules-db    (SOCI; PostgreSQL/MySQL/SQLite)
  └── include/fastrules/repository.hpp   shared repository interface
docs/                Jekyll/GitHub Pages docs (api/, advanced/, extensions/)
cmake/               Config templates, CodeCoverage, copy_test_dlls helper
.github/workflows/   CI: ci.yml, lint.yml, code-quality.yml, coverage.yml, release.yml
```

Each extension mirrors the core layout: `src/`, `include/fastrules/`, `tests/`,
`examples/`, and its own `CMakeLists.txt`.

## Architecture & Key Concepts

The public umbrella header is `include/fastrules/fastrules.hpp` (include as
`#include <fastrules.hpp>`). It pulls in all core components. Core subsystems:

- **Rule** (`rule.hpp`) — the fundamental condition/action pair. Has an `id`,
  optional `name` (used for dependencies), a Lua `expression` (evaluates
  truthy/falsy), an optional Lua `action` (runs on success), `priority`,
  `isActive`, optional `timeout`, optional `cacheDuration`, child rules, and
  `dependsOnRuleName`. Construct directly or via the fluent `Rule::Builder` /
  `Rule::create(...)`. Predicate factories: `isNotNull`, `greaterThan`,
  `lessThan`, `equals`, `matchesRegex`, `contains`.
- **Rule execution lifecycle**: active check → rate limit → param validation →
  children (bottom-up; any child failure aborts the parent) → expression →
  action → cache → store result in context.
- **Workflow** (`workflow.hpp`) — orchestrates ordered rule execution with
  dependency resolution; `compile()` then `execute()` / `executeParallel()`.
- **RuleContext** (`rule_context.hpp`) — holds intermediate results & variables;
  thread-safe via `shared_mutex`.
- **RuleResult** (`rule_result.hpp`) — success/failure + metadata.
- **LuaEngine** (`lua_engine.hpp`) — compiles/executes Lua; `clone()` for
  per-thread states. **LuaBackend / LuaBridge3Backend** abstract the binding.
- **TypeRegistry** (`type_registry.hpp`) + `type_registration_macro.hpp` — bind
  C++ structs/enums into Lua. **ActionCallbacks** (`action_callback.hpp`) —
  register C++ functions callable from Lua actions.
- **Async/parallel**: `async_workflow.hpp`, `engine_pool.hpp`,
  `work_stealing_thread_pool.hpp`, `streaming_result.hpp`. See
  `docs/parallel-execution.md` for `executeParallel` vs `AsyncWorkflow` guidance.
- **Security**: `sandbox.hpp` (env removal, memory/instruction limits),
  `expression_validator.hpp` (dangerous-pattern detection), `input_validator.hpp`,
  `parameter_validator.hpp`, `rate_limiter.hpp`.
- **Persistence/versioning**: `aot_compiler.hpp` (pre-compile workflows to binary
  bundles), `rule_versioning.hpp` (semver history + rollback).
- **Observability**: `execution_tracer.hpp` (JSON export), `performance_counters.hpp`,
  `logger.hpp` (spdlog-backed).
- **C API** (`fastrules.h`, `src/c_api.cpp`) — always part of core; powers the
  C#/Python FFI examples. Symbols `fastrules_*`, exported via `fastrules_export.hpp`.

### Thread-safety rules (important when changing execution code)
- `LuaEngine`: thread-safe for concurrent execution **only via clones** — one
  engine per thread.
- `Rule`: construction and `compile()` are **not** thread-safe (compile once);
  execution is safe if each thread uses its own engine. Cache ops are mutex-guarded.
- `Workflow`: thread-safe for execution after compilation.
- `RuleContext`: thread-safe.

## Build, Test, Run

Requirements: **CMake 3.28+**, **C++23 compiler** (GCC 13+, Clang 17+, MSVC 2022+),
Git. Dependencies come from **vcpkg manifest mode** (auto-enabled when `vcpkg.json`
is present): `lua` (≥5.4.6), `spdlog`, plus per-feature `doctest`, `nlohmann-json`,
`pugixml`, `soci`. **LuaBridge3** is header-only and pulled via `FetchContent`.

### Core build options (all default OFF)
| Option | Effect |
|---|---|
| `FASTRULES_BUILD_TESTS` | Build `fastrules_tests` (doctest) |
| `FASTRULES_BUILD_EXAMPLES` | Build the examples |
| `FASTRULES_BUILD_EXTENSIONS` | Build JSON + XML + DB extensions (all together) |
| `FASTRULES_BUILD_STRESS_TESTS` | Build `stress/` |
| `FASTRULES_BUILD_COVERAGE` | Coverage instrumentation (MSVC: OpenCppCoverage) |
| `FASTRULES_BUILD_SHARED` | Build shared lib (DLL/.so/.dylib) instead of static |

Note: enabling `FASTRULES_BUILD_EXTENSIONS` turns on JSON, XML **and** DB
(`FASTRULES_BUILD_*_EXT` are derived from it). CI passes `-DFASTRULES_BUILD_DB=OFF`
on Ubuntu because SOCI isn't in apt.

### Typical commands
```bash
# Configure (core + tests + examples + extensions)
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON -DFASTRULES_BUILD_EXAMPLES=ON -DFASTRULES_BUILD_EXTENSIONS=ON

# Build
cmake --build build --config Release

# Test
ctest --test-dir build --output-on-failure
# or run the binary directly:
#   Linux/macOS: ./build/Release/fastrules_tests
#   Windows:     .\build\Release\fastrules_tests.exe
```

### CMake presets (preferred where available)
`CMakePresets.json` provides `linux-default`, `macos-default`, `windows-default`
(plus `windows-release`/`windows-debug`), and sanitizer presets `linux-asan`
(ASan+UBSan, Debug) and `linux-tsan` (ThreadSanitizer, Release). Example:
```bash
cmake --preset linux-default
cmake --build --preset linux-release
ctest --preset linux-test
```
`build.ps1` is the convenience wrapper for Windows.

### Running a single test
doctest filters by name/tag, e.g. `./build/Release/fastrules_tests "Rule basic creation"`
or `... --test-case="*cache*"`.

## Conventions

- **Language**: C++23. Modern features encouraged (concepts, ranges,
  designated initializers, coroutines). Prefer `std::optional` over raw
  pointers, value semantics over reference members, `noexcept` on hot paths,
  `[[nodiscard]]` on pure accessors/factories.
- **Namespace**: everything lives in `namespace fastrules`.
- **Formatting**: `.clang-format` (LLVM base, 4-space indent, 120-col limit,
  attached braces, left-aligned pointers/refs, no namespace indentation,
  sorted includes). Run clang-format (v17) before committing.
- **Linting**: `.clang-tidy` enables bugprone/cppcoreguidelines/modernize/
  performance/portability/readability with a curated suppression list;
  `HeaderFilterRegex` targets `include/fastrules/.*`. `.editorconfig` governs
  whitespace (LF, final newline, 4-space C++, 2-space yaml/json/cmake).
- **Headers**: `#pragma once`, Doxygen `@file`/`@brief` blocks. Public headers
  listed explicitly in `CMakeLists.txt` (`FASTRULES_PUBLIC_HEADERS`) — when you
  add a public header, add it there too.
- **pre-commit** (`.pre-commit-config.yaml`): trailing-whitespace, EOF fixer,
  yaml/json checks, clang-format, cmake-format/cmake-lint, and cppcheck
  (`--std=c++20 --enable=all`). Install with `pre-commit install`.
- **Tests**: framework is **doctest** (not Catch2, despite Catch-style
  `TEST_CASE` syntax). `tests/test_main.cpp` owns the `main()` via
  `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`. Use `CHECK`/`REQUIRE`/`CHECK_EQ`.
  Core tests are globbed (`tests/*.cpp`) — new files are picked up automatically.
  Every new feature must ship with tests.

## Adding Things

- **New core source**: add the `.cpp` to `FASTRULES_CORE_SOURCES` in
  `CMakeLists.txt`; add the public header to `FASTRULES_PUBLIC_HEADERS`.
- **New example**: add to the `FASTRULES_EXAMPLES` list in `CMakeLists.txt`
  (links `fastrules` automatically).
- **New extension**: create `extensions/<name>/{src,include/fastrules,tests}/`
  with its own `CMakeLists.txt`, `add_subdirectory` it from
  `extensions/CMakeLists.txt`, and add a vcpkg feature in `vcpkg.json` plus a
  Conan option in `conanfile.py`. Guideline: extensions **only add** to the API,
  never modify core; keep extension version in lockstep with core. See
  `extensions/README.md` and `extensions/IMPLEMENTATION.md`.

## CI Expectations (what must stay green)

CI (`.github/workflows/`) runs on push/PR to `main`:
- **Windows** (MSVC) for both `lua54` and `luajit` backends, with extensions + DB.
- **Ubuntu** (GCC) with extensions, DB off (no SOCI in apt).
- **Sanitizers**: TSan and ASan+UBSan on Ubuntu/Clang (extensions off).
- **Conan** `test_package` with JSON+XML.
- Separate workflows for lint, code-quality, and coverage (Codecov).

Before pushing, ensure: tests pass, no new compiler warnings (treated seriously
on GCC/Clang/MSVC `/W4`), clang-format clean, and `CHANGELOG.md` updated under
`[Unreleased]` for user-visible changes (Keep a Changelog + SemVer; project
version is `0.2.0` in `CMakeLists.txt`).

## Git Workflow for This Repo

- Branch from `master` (or the designated feature branch); never push directly
  to `master` without explicit permission.
- Conventional, descriptive commit messages; reference the affected subsystem.
- Do **not** open a pull request unless explicitly asked.
- Update docs (`README.md`, `docs/`, `CHANGELOG.md`) alongside public API changes.

## Quick Reference: Minimal Usage

```cpp
#include <fastrules.hpp>
using namespace fastrules;

LuaEngine engine;

auto rule = Rule::Builder(1)          // returns std::shared_ptr<Rule>
    .withExpression("age >= 18")
    .withAction("status = 'adult'")
    .build();

Workflow workflow;
workflow.rules.push_back(rule);        // rules is std::vector<std::shared_ptr<Rule>>
workflow.compile(engine);

std::vector<RuleParameter> params;
params.emplace_back("age", 25);
auto results = workflow.execute(engine, params);  // results[0].isSuccess()
```

For deeper docs see `docs/` (architecture, concepts, getting_started,
parallel-execution, security, c_api, extensions/) and the published site at
https://asulwer.github.io/FastRules.
