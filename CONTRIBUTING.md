# Contributing to FastRules

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/fastrules.git`
3. Create a branch: `git checkout -b feature/my-feature`

## Building

```bash
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON -DFASTRULES_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Code Style

- **C++23** features welcome (concepts, ranges, etc.)
- Use `std::optional` instead of raw pointers where possible
- Add `noexcept` to hot paths
- Prefer value semantics over reference members

## Testing

All new features must include tests:

```cpp
TEST_CASE("My new feature works", "[feature]") {
    // Arrange
    LuaEngine engine;
    Rule rule;
    rule.id = "test";
    rule.expression = "true";
    rule.compile(engine);

    // Act
    auto result = rule.execute(engine, ctx, params);

    // Assert
    REQUIRE(result.isSuccess());
}
```

Run tests before submitting:
```bash
# Windows
.\build\Release\fastrules_tests.exe

# Linux/macOS
./build/Release/fastrules_tests
```

## Documentation

- Update `README.md` if changing public API
- Add examples to `examples/` for new features
- Update `docs/` for architectural changes

## Pull Request Checklist

- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] No compiler warnings (GCC/Clang/MSVC)
- [ ] Code follows existing style

## Reporting Issues

Include:
- OS and compiler version
- CMake version
- Minimal reproducible example
- Expected vs actual behavior
