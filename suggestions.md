# FastRules Improvement Suggestions

*Documented: 2026-06-15*

---

## 1. ✅ Documentation Organization (IN PROGRESS)

**Status:** Already well-structured, minor cleanup needed

**Current Structure:**
```
docs/
├── _config.yml              # Jekyll config
├── index.md                 # Landing page
├── getting_started.md       # Quick start
├── concepts.md              # Core concepts
├── architecture.md          # System architecture
├── performance.md           # Performance guide
├── security.md              # Security considerations
├── logging.md               # Logging guide
├── lua-compatibility.md     # Lua version support
├── coverage.md              # Test coverage
├── examples.md              # Examples index
├── adaptive-execution.md    # Adaptive execution guide
├── parallel-execution.md    # Parallel execution guide
├── api/                     # API documentation
│   ├── index.md
│   ├── workflow.md
│   ├── rule.md
│   ├── lua_engine.md
│   ├── async_workflow.md
│   ├── type_registry.md
│   ├── json_loader.md
│   └── action_callbacks.md
├── extensions/              # Extension docs
│   ├── index.md
│   ├── json.md
│   ├── xml.md
│   └── db.md
└── advanced/                # Advanced topics
    ├── index.md
    ├── custom-methods.md
    └── aot-and-versioning.md
```

**Suggested Improvements:**
- [ ] Add `troubleshooting.md` for common issues
- [ ] Add `migration.md` for version upgrades
- [ ] Add `contributing.md` for developers
- [ ] Consolidate `examples.md` with actual examples folder
- [ ] Add API version changelog

---

## 2. 🧪 Test Coverage Gaps

**Priority:** High
**Effort:** Medium

**Current Gaps:**
- [ ] **Snapshot serializers**: Need tests for JSON and XML snapshot serializers
- [ ] **Edge cases**: Missing tests for:
  - Invalid Lua expressions (syntax errors)
  - Rule timeout enforcement
  - Circular dependencies between rules
  - Maximum recursion depth
  - Concurrent modifications
- [ ] **Performance regression tests**: Automated benchmarks in CI
- [ ] **Memory leak tests**: Valgrind/ASAN integration
- [ ] **Fuzzing**: Randomized input testing for expression parser

**Implementation:**
```cpp
// Example: Add to tests/test_edge_cases.cpp
TEST_CASE("Rule timeout enforcement", "[edge][timeout]") {
    Rule rule;
    rule.expression = "while true do end";  // Infinite loop
    rule.timeout = std::chrono::milliseconds(100);
    
    // Should throw/timeout, not hang forever
}
```

---

## 3. 🔧 Code Quality Improvements

**Priority:** Medium
**Effort:** Low

**Static Analysis:**
- [ ] Enable stricter compiler warnings (`-Wall -Wextra -Werror`)
- [ ] Add clang-tidy configuration
- [ ] Add cppcheck to CI
- [ ] Enable address sanitizer (ASAN) in debug builds

**Formatting:**
- [ ] Add `.clang-format` configuration file
- [ ] Add `.editorconfig` for IDE consistency
- [ ] Pre-commit hooks for formatting

**Suggested .clang-format:**
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
AllowShortFunctionsOnASingleLine: Empty
SortIncludes: true
```

---

## 4. 🚀 Performance Enhancements

**Priority:** High
**Effort:** Medium-High

**AOT Compilation:**
- [ ] Pre-compile common Lua patterns at build time
- [ ] Cache compiled chunks in `.fastrules_cache/` directory
- [ ] Add `compileToBytecode()` API for distribution

**Expression Caching:**
```cpp
// Cache compiled Lua chunks
class LuaExpressionCache {
    std::unordered_map<std::string, lua_CompiledChunk> cache_;
public:
    auto getOrCompile(const std::string& expression) {
        if (cache_.contains(expression)) return cache_[expression];
        auto compiled = compileExpression(expression);
        cache_[expression] = compiled;
        return compiled;
    }
};
```

**Memory Pooling:**
- [ ] Pool `RuleContext` objects (avoid frequent allocations)
- [ ] Pool `RuleResult` vectors
- [ ] Custom allocator for small workflows

**Benchmarks Needed:**
| Optimization | Expected Gain |
|-------------|---------------|
| Expression caching | 30-50% for repeated rules |
| Memory pooling | 10-20% reduction in allocations |
| AOT compilation | 2-5x faster startup |

---

## 5. 🔒 Security Hardening

**Priority:** High
**Effort:** Medium

**Input Validation:**
- [ ] Sanitize Lua expressions (prevent injection)
- [ ] Whitelist allowed Lua functions
- [ ] Regex-based expression validation before compilation

**Timeout Enforcement:**
```cpp
// Hard timeout that can't be bypassed
class RuleExecutor {
    static constexpr auto MAX_EXECUTION_TIME = std::chrono::seconds(30);
    
    template<typename F>
    auto executeWithTimeout(F&& fn) {
        return std::async(std::launch::async, [&]() {
            auto future = std::async(std::launch::async, fn);
            if (future.wait_for(MAX_EXECUTION_TIME) == std::future_status::timeout) {
                throw RuleTimeoutException();
            }
            return future.get();
        }).get();
    }
};
```

**Sandboxing:**
- [ ] Restrict Lua `io` and `os` modules
- [ ] Disable `loadstring`/`loadfile`
- [ ] Memory limits per rule execution

---

## 6. 🎯 Developer Experience

**Priority:** Medium
**Effort:** Medium

**CLI Tool:**
```bash
# FastRules CLI
fastrules validate rules.json          # Validate rule syntax
fastrules test rules.json --params x=5 # Test execution
fastrules benchmark rules.json         # Performance benchmark
fastrules compile rules.json --output bytecode.bin  # AOT compile
```

**VS Code Extension:**
- [ ] Syntax highlighting for `.rules` files
- [ ] IntelliSense for rule expressions
- [ ] Linting for common mistakes
- [ ] Debug adapter for rule execution

**Docker Image:**
```dockerfile
# Dockerfile.fastrules
FROM ubuntu:24.04
RUN apt-get install -y lua5.4
COPY build/libfastrules.so /usr/lib/
COPY fastrules-cli /usr/bin/
ENTRYPOINT ["fastrules"]
```

---

## 7. 📦 Distribution & Packaging

**Priority:** Medium
**Effort:** High

**Package Managers:**
- [ ] **vcpkg**: Add to Microsoft vcpkg registry
- [ ] **Conan**: Create conanfile.py for ConanCenter
- [ ] **Homebrew**: Formula for macOS
- [ ] **NuGet**: C# bindings package
- [ ] **PyPI**: Python bindings package

**vcpkg Port:**
```json
// ports/fastrules/vcpkg.json
{
  "name": "fastrules",
  "version": "1.0.0",
  "description": "High-performance C++ rule engine",
  "dependencies": ["lua", "spdlog"]
}
```

**GitHub Actions:**
- [ ] Automated builds for Windows/Linux/macOS
- [ ] Automated tests with coverage reporting
- [ ] Automated releases with artifacts
- [ ] Automated package publishing

**Changelog:**
- [ ] Keep `CHANGELOG.md` with semantic versioning
- [ ] Automated changelog generation from commits

---

## Implementation Priority Matrix

| Priority | Item | Effort | Impact |
|----------|------|--------|--------|
| 🔴 High | Security hardening | Medium | Critical |
| 🔴 High | Test coverage gaps | Medium | High |
| 🟡 Medium | Performance enhancements | High | High |
| 🟡 Medium | Developer experience | Medium | Medium |
| 🟢 Low | Code quality/formatting | Low | Medium |
| 🟢 Low | Documentation polish | Low | Low |

---

## Quick Wins (This Week)

1. Add `.clang-format` file
2. Add `troubleshooting.md` to docs
3. Create basic CLI tool scaffold
4. Add test for rule timeout enforcement
5. Enable ASAN in debug builds

---

*Last updated: 2026-06-15*
