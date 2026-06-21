#Requires -Version 5.1
<#
.SYNOPSIS
    Build FastRules with Visual Studio 2022.
.DESCRIPTION
    Thin wrapper around CMake. Configures, builds, and optionally tests FastRules.
    All heavy lifting (dependency fetching, DLL copying, test setup) is handled by CMake.

.PARAMETER Clean
    Remove build_vs directory and regenerate from scratch.

.PARAMETER NoBuild
    Only configure CMake, do not build.

.PARAMETER NoTest
    Skip running tests after build.

.PARAMETER Configuration
    Build configuration(s): Debug, Release, or Both. Default is Debug.

.EXAMPLE
    .\build.ps1
    Configure and build Debug.

.EXAMPLE
    .\build.ps1 -Configuration Release -NoTest
    Build Release without running tests.

.EXAMPLE
    .\build.ps1 -Configuration Both
    Build both Debug and Release.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$NoBuild,
    [switch]$NoTest,
    [ValidateSet('Debug', 'Release', 'Both')]
    [string]$Configuration = 'Debug'
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
    Write-Host 'Cleaning build directory...' -ForegroundColor Yellow
    $buildVs = Join-Path $root 'build_vs'
    if (Test-Path $buildVs) {
        Remove-Item $buildVs -Recurse -Force
        Write-Host "  Removed: build_vs"
    }
    Write-Host 'Done.' -ForegroundColor Green
    exit 0
}

# ============================================================================
# Configuration
# ============================================================================
$configs = if ($Configuration -eq 'Both') { @('Debug', 'Release') } else { @($Configuration) }

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "FastRules - Building with Visual Studio 2022" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration:"
Write-Host "  Generator:      Visual Studio 17 2022"
Write-Host "  Build mode:     $Configuration"
Write-Host "  C++ Standard:   23"
Write-Host "  Tests:          ON"
Write-Host "  Stress:         ON"
Write-Host "  Examples:       ON"
Write-Host "  Extensions:     ON"
Write-Host ""

$buildDir = Join-Path $root 'build_vs'

$cmakeArgs = @(
    '-B', $buildDir
    '-S', $root
    '-G', 'Visual Studio 17 2022'
    '-DCMAKE_CXX_STANDARD=23'
    '-DCMAKE_CXX_STANDARD_REQUIRED=ON'
    '-DFASTRULES_BUILD_STRESS_TESTS=ON'
	'-DFASTRULES_BUILD_TESTS=ON'
    '-DFASTRULES_BUILD_EXAMPLES=ON'
    '-DFASTRULES_BUILD_EXTENSIONS=ON'
)

# Pass the vcpkg toolchain when the CMake cache does not already contain it.
# This avoids the harmless "variable not used" warning on reconfigures while
# still ensuring vcpkg installs dependencies on a fresh build.
$toolchain = Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
$cacheFile = Join-Path $buildDir 'CMakeCache.txt'
$toolchainCached = $false
if (Test-Path $cacheFile) {
    $cachedLine = Select-String -Path $cacheFile -Pattern '^CMAKE_TOOLCHAIN_FILE:FILEPATH=(.*)$' | Select-Object -First 1
    if ($cachedLine) {
        $cachedToolchain = $cachedLine.Matches.Groups[1].Value
        if ((Resolve-Path $cachedToolchain).Path -eq (Resolve-Path $toolchain).Path) {
            $toolchainCached = $true
        }
    }
}
if (-not $toolchainCached -and (Test-Path $toolchain)) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
}

# vcpkg manifest features must be passed as a cache variable because vcpkg
# evaluates them before CMakeLists.txt runs.
$vcpkgFeatures = @('tests', 'json', 'xml', 'db')
$cmakeArgs += "-DVCPKG_MANIFEST_FEATURES=$($vcpkgFeatures -join ';')"

# ============================================================================
# Step 1: Configure
# ============================================================================
Write-Host "[1/3] Configuring CMake..." -ForegroundColor Yellow
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
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
foreach ($config in $configs) {
    Write-Host ""
    Write-Host "[2/3] Building $config configuration..." -ForegroundColor Yellow
    & cmake --build $buildDir --config $config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed for $config configuration."
        exit 1
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
foreach ($config in $configs) {
    Write-Host ""
    Write-Host "[3/3] Running tests ($config)..." -ForegroundColor Yellow
    & ctest --test-dir $buildDir -C $config --output-on-failure --timeout 120
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Some tests failed for $config."
    }
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Done! Solution: $buildDir\fastrules.sln" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
