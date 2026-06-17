---
layout: default
title: Code Coverage Setup
nav_order: 12
---

# Code Coverage Setup

## Windows + MSVC + OpenCppCoverage

### Installation

```powershell
# Install OpenCppCoverage
choco install opencppcoverage

# Or download from GitHub releases
# https://github.com/OpenCppCoverage/OpenCppCoverage/releases
```

### Build with Coverage

```powershell
# Configure with coverage enabled
cmake -B build -S . -DFASTRULES_BUILD_COVERAGE=ON -DFASTRULES_BUILD_TESTS=ON

# Build Debug configuration (required for accurate coverage)
cmake --build build --config Debug

# Run tests with coverage
cmake --build build --target coverage
```

### Output

- HTML report: `build/coverage/html/index.html`
- Cobertura XML: `build/coverage/cobertura.xml` (for CI integration)

### Manual Run

```powershell
# Run OpenCppCoverage directly
OpenCppCoverage.exe `
    --sources "C:\path\to\fastrules\include" `
    --sources "C:\path\to\fastrules\src" `
    --sources "C:\path\to\fastrules\extensions" `
    --export_type html: coverage\html `
    --export_type cobertura: coverage\cobertura.xml `
    --working_dir "build" `
    -- ctest -C Debug --output-on-failure
```

## Linux/macOS + GCC/Clang + lcov/gcov

### Installation

```bash
# Ubuntu/Debian
sudo apt-get install lcov

# macOS
brew install lcov
```

### Build with Coverage

```bash
# Configure
cmake -B build -S . -DFASTRULES_BUILD_COVERAGE=ON -DFASTRULES_BUILD_TESTS=ON

# Build
cmake --build build

# Run tests
cd build && ctest --output-on-failure

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info
```

## CI Integration

### GitHub Actions

Add to `.github/workflows/ci.yml`:

```yaml
- name: Coverage (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    choco install opencppcoverage
    cmake -B build -S . -DFASTRULES_BUILD_COVERAGE=ON
    cmake --build build --config Debug
    cmake --build build --target coverage
  
- name: Upload Coverage
  uses: codecov/codecov-action@v3
  with:
    files: ./build/coverage/cobertura.xml
```

## Interpreting Results

- **Line coverage**: Percentage of executable lines executed
- **Function coverage**: Percentage of functions called
- **Branch coverage**: Percentage of control flow branches taken

Target: >80% line coverage for production code.
