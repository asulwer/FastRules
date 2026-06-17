---
layout: default
title: Contributing
nav_order: 17
---

# Contributing to FastRules

Thank you for your interest in contributing to FastRules! This document provides guidelines for contributing code, documentation, and reporting issues.

## Code of Conduct

- Be respectful and constructive in all interactions
- Welcome newcomers and help them get started
- Focus on what's best for the community and project
- Accept constructive criticism gracefully

## How to Contribute

### Reporting Bugs

Before reporting a bug:

1. Check [existing issues](https://github.com/asulwer/FastRules/issues) to avoid duplicates
2. Try the [troubleshooting guide](troubleshooting.md)
3. Isolate the problem with minimal code

When reporting:

```markdown
**Environment:**
- OS: [e.g., Ubuntu 22.04]
- Compiler: [e.g., GCC 13.1]
- FastRules version: [e.g., 0.2.0]
- CMake version: [e.g., 3.28]

**Description:**
Clear description of the bug

**Reproduction:**
```cpp
// Minimal code that reproduces the issue
LuaEngine engine;
auto rule = Rule::create(1, "true").build();
// ... etc
```

**Expected behavior:**
What you expected to happen

**Actual behavior:**
What actually happened (include error messages)
```

### Suggesting Features

Feature requests are welcome! Please include:

- Use case: What problem does this solve?
- API design: How would it look to users?
- Alternatives: What workarounds exist?
- Priority: Is this blocking your work?

### Contributing Code

#### Development Setup

```bash
# Fork and clone
git clone https://github.com/YOUR_USERNAME/fastrules.git
cd fastrules

# Create feature branch
git checkout -b feature/my-feature

# Configure build
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DFASTRULES_BUILD_TESTS=ON \
    -DFASTRULES_BUILD_EXAMPLES=ON

# Build
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure
```

#### Code Style

**C++ Style:**
- Follow existing code style in files you edit
- Use 4 spaces for indentation (no tabs)
- Maximum line length: 100 characters
- Use `snake_case` for variables, `camelCase` for functions, `PascalCase` for classes

**Example:**
```cpp
class RuleBuilder {
public:
    RuleBuilder& withExpression(const std::string& expr) {
        rule_.expression = expr;
        return *this;
    }

private:
    Rule rule_;
};
```

**Documentation:**
- Add Doxygen comments for public APIs
- Update relevant markdown docs
- Add examples for new features

**Testing:**
- Add tests for bug fixes
- Add tests for new features
- Ensure tests pass: `ctest --output-on-failure`

#### Commit Messages

Use conventional commits format:

```
type(scope): description

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style (formatting, no logic change)
- `refactor`: Code refactoring
- `test`: Test changes
- `chore`: Build/dependency changes

Examples:
```
feat(rule): add timeout support to rules

Rules can now specify execution timeouts to prevent
runaway expressions.

Closes #123
```

```
fix(engine): prevent use-after-free in lua callbacks

Lua callbacks were capturing references that could be
invalidated. Now uses shared_ptr for safety.

Fixes #456
```

#### Pull Request Process

1. **Before submitting:**
   - Run tests: `ctest --output-on-failure`
   - Run examples to verify they still work
   - Update documentation if needed
   - Add yourself to CONTRIBUTORS.md (if it exists)

2. **PR Description should include:**
   - What changes were made
   - Why they were made
   - How to test the changes
   - Link to related issues

3. **Review process:**
   - Maintainers will review within a few days
   - Address review comments
   - Squash commits if requested

4. **After merge:**
   - Delete your feature branch
   - Celebrate! 🎉

## Project Structure

```
fastrules/
├── docs/               # Documentation (GitHub Pages)
├── examples/           # Example programs
├── extensions/         # Optional extensions
│   ├── json/
│   ├── xml/
│   └── db/
├── include/            # Public headers
│   └── fastrules/
├── src/                # Implementation
├── tests/              # Unit tests
├── CMakeLists.txt      # Build configuration
└── README.md
```

## Areas Needing Help

### High Priority

1. **Performance optimizations**
   - Profile and optimize hot paths
   - Reduce memory allocations
   - Improve cache locality

2. **Additional test coverage**
   - Edge cases
   - Error paths
   - Platform-specific issues

3. **Documentation**
   - More examples
   - Tutorial content
   - API reference improvements

### Medium Priority

4. **New extensions**
   - YAML support
   - Protocol Buffers support
   - Additional database backends

5. **Language bindings**
   - Python improvements
   - C# improvements
   - Rust bindings

6. **Tooling**
   - IDE plugins
   - Debugging tools
   - Performance profilers

### Low Priority

7. **Code modernization**
   - C++26 features when available
   - Refactoring for clarity
   - Static analysis improvements

## Development Guidelines

### Adding New Features

1. **Design first:** Open an issue to discuss the design
2. **Keep core minimal:** Extensions are preferred for non-essential features
3. **Maintain backward compatibility:** Deprecate, don't break
4. **Document:** Update docs before PR is merged

### Adding New Extensions

Extensions should:
- Be in `extensions/<name>/`
- Have their own CMakeLists.txt
- Include tests
- Include documentation
- Not add dependencies to core

Example structure:
```
extensions/yaml/
├── CMakeLists.txt
├── include/
│   └── fastrules/
│       └── yaml_loader.hpp
├── src/
│   └── yaml_loader.cpp
├── tests/
│   └── test_yaml_loader.cpp
└── examples/
    └── example_yaml.cpp
```

### Testing Philosophy

- **Unit tests:** Test individual components in isolation
- **Integration tests:** Test component interactions
- **Example programs:** Serve as integration tests and documentation

Test categories:
- Correctness: Does it produce right results?
- Performance: Is it fast enough?
- Robustness: Does it handle errors gracefully?
- Concurrency: Is it thread-safe?

## Release Process

Maintainers only:

1. Update version in CMakeLists.txt
2. Update CHANGELOG.md
3. Tag release: `git tag v0.X.Y`
4. Push tag: `git push origin v0.X.Y`
5. Create GitHub release with notes
6. Update vcpkg and Conan packages

## Questions?

- **General questions:** Open a [Discussion](https://github.com/asulwer/FastRules/discussions)
- **Security issues:** Email maintainers directly (don't open public issue)
- **Private questions:** DM maintainers on relevant platforms

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
