#Requires -Version 5.1
<#
.SYNOPSIS
    Generate Visual Studio 2022 Solution for FastRules
.DESCRIPTION
    This script generates a complete Visual Studio 2022 solution including:
      - Core fastrules library
      - C API shared library (for Python/C# bindings)
      - Extensions (JSON, XML; DB when SOCI available and -BuildDB passed)
      - Test suite (fastrules_tests)
      - All examples
      - All dependencies (fetched automatically)

    The generated solution will be in the build_vs\ folder.
    Open build_vs\fastrules.sln in Visual Studio 2022.

.PARAMETER Clean
    Remove all build directories and regenerate from scratch.

.PARAMETER UseLuaJIT
    Use LuaJIT instead of PUC-Rio Lua (requires manual build on Windows).

.PARAMETER NoBuild
    Only configure CMake, do not build.

.PARAMETER NoTest
    Skip running tests after build.

.PARAMETER BuildDB
    Enable database persistence extension (requires SOCI installed).

.PARAMETER Configuration
    Build configuration: Debug, Release, or All (both). Default is Release.

.EXAMPLE
    .\build.ps1
    Generates solution with default settings (Release).

.EXAMPLE
    .\build.ps1 -Configuration Debug
    Build Debug configuration only.

.EXAMPLE
    .\build.ps1 -Configuration All
    Build both Debug and Release.

.EXAMPLE
    .\build.ps1 -Clean
    Clean all build dirs and regenerate from scratch.

.EXAMPLE
    .\build.ps1 -UseLuaJIT
    Use LuaJIT instead of PUC-Rio Lua.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$UseLuaJIT,
    [switch]$NoBuild,
    [switch]$NoTest,
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $root) { $root = Get-Location }

# Auto-detect vcpkg from Visual Studio installation
if (-not $env:VCPKG_ROOT) {
    $vsVcpkg = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\vcpkg"
    if (Test-Path $vsVcpkg) {
        $env:VCPKG_ROOT = $vsVcpkg
        Write-Host "Auto-detected vcpkg: $vsVcpkg" -ForegroundColor Gray
    } else {
        Write-Warning "VCPKG_ROOT not set and vcpkg not found in VS installation"
    }
}

# ============================================================================
# Clean
# ============================================================================
if ($Clean) {
    Write-Host 'Cleaning build directories...' -ForegroundColor Yellow
    $dirs = @('build_vs', 'build', 'build_test', 'build_cov', 'build_coverage',
              'build_luajit', 'build_luajit2', 'build_vstudio', 'Testing')
    foreach ($d in $dirs) {
        $p = Join-Path $root $d
        if (Test-Path $p) {
            Remove-Item $p -Recurse -Force
            Write-Host "  Removed: $d"
        }
    }
    Write-Host 'Done.' -ForegroundColor Green
    exit 0
}

# ============================================================================
# Check prerequisites
# ============================================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "FastRules - Generating Visual Studio 2022 Solution" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# Check VS 2022
$vsPaths = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe"
)
$vsFound = $false
foreach ($p in $vsPaths) {
    if (Test-Path $p) { $vsFound = $true; break }
}
if (-not $vsFound) {
    Write-Error "Visual Studio 2022 not found. Please install VS 2022 with C++ workload."
    exit 1
}

# Check CMake
try { $cmakeVer = (cmake --version 2>$null)[0] } catch {}
if (-not $cmakeVer) {
    Write-Error "CMake not found in PATH. Please install CMake 3.28 or later."
    exit 1
}

# ============================================================================
# Configuration
# ============================================================================
Write-Host "Configuration:"
Write-Host "  Generator:      Visual Studio 17 2022"
Write-Host "  Configuration:  $Configuration"
Write-Host "  C++ Standard:   23"
Write-Host "  Tests:          ON"
Write-Host "  Examples:       ON"
Write-Host "  Extensions:     ON (JSON, XML, DB)"
Write-Host "  C API:          ON (included in core library)"
Write-Host "  LuaJIT:         $(if ($UseLuaJIT) { 'ON' } else { 'OFF' })"
Write-Host ""

$buildDir = Join-Path $root 'build_vs'

# CMake arguments
$cmakeArgs = @(
    '-B', $buildDir
    '-S', $root
    '-G', 'Visual Studio 17 2022'
    '-DCMAKE_CXX_STANDARD=23'
    '-DCMAKE_CXX_STANDARD_REQUIRED=ON'
    '-DFASTRULES_BUILD_TESTS=ON'
    '-DFASTRULES_BUILD_EXAMPLES=ON'
    '-DFASTRULES_BUILD_EXTENSIONS=ON'
    '-DFASTRULES_BUILD_SHARED=ON'
    '-DFASTRULES_BUILD_C_API=ON'
)

# Add vcpkg toolchain if available
if ($env:VCPKG_ROOT) {
    $toolchain = Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
    if (Test-Path $toolchain) {
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    }
}

# Add vcpkg features based on options
$vcpkgFeatures = @('tests', 'json', 'xml', 'db')
$featureList = $vcpkgFeatures -join ';'
$cmakeArgs += "-DVCPKG_MANIFEST_FEATURES=$featureList"

if ($UseLuaJIT) {
    $cmakeArgs += '-DFASTRULES_USE_LUAJIT=ON'
}

# ============================================================================
# Step 1: Configure
# ============================================================================
Write-Host "[1/3] Configuring CMake..." -ForegroundColor Yellow
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed. If dependency fetching hangs, ensure internet access."
    exit 1
}

if ($NoBuild) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host "Configure complete. Solution: $buildDir\fastrules.sln" -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Green
    exit 0
}

# ============================================================================
# Step 2: Build
# ============================================================================
$configsToBuild = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }

foreach ($config in $configsToBuild) {
    Write-Host ""
    Write-Host "[2/3] Building $config configuration..." -ForegroundColor Yellow
    & cmake --build $buildDir --config $config

    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Build had errors for $config configuration."
    }
}

if ($NoTest) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host "Build complete. Solution: $buildDir\fastrules.sln" -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Green
    exit 0
}

# ============================================================================
# Step 3: Test
# ============================================================================
$testConfig = if ($Configuration -eq 'All') { 'Release' } else { $Configuration }
Write-Host ""
Write-Host "[3/3] Running tests ($testConfig)..." -ForegroundColor Yellow
& ctest --test-dir $buildDir --build-config $testConfig --output-on-failure

# ============================================================================
# Summary
# ============================================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Done! Open the solution:" -ForegroundColor Green
Write-Host "  $buildDir\fastrules.sln" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Available build targets:"
Write-Host "  - fastrules           : Core library with C API exports"
Write-Host "  - fastrules_tests     : Test suite"
Write-Host "  - fastrules-json      : JSON persistence extension"
Write-Host "  - fastrules-xml       : XML persistence extension"
Write-Host "  - fastrules-db        : Database persistence extension (SOCI)"
Write-Host "  - csharp_example      : C# example (run from VS or: dotnet run)"
Write-Host "  - python_example      : Python example (run from VS or: python fastrules_example.py)"
Write-Host "  - All C++ examples    : simple, core_only, workflow, json, xml, etc."
Write-Host ""
Write-Host "C# & Python examples:"
Write-Host "  The C API DLL and dependencies are auto-copied to:"
Write-Host "    - examples\csharp_example\"
Write-Host "    - examples\python_example\"
Write-Host "  Run: dotnet run (C#) or python fastrules_example.py (Python)"
Write-Host ""
Write-Host "Other options:"
Write-Host "  .\build.ps1 -UseLuaJIT"
Write-Host ""
