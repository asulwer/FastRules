#Requires -Version 5.1
<#
.SYNOPSIS
    Build FastRules with Visual Studio 2022.
.DESCRIPTION
    Thin wrapper around CMake. Configures, builds, and optionally tests FastRules.
    All heavy lifting (dependency fetching, DLL copying, test setup) is handled by CMake.

.PARAMETER Clean
    Remove build artifacts (but preserve vcpkg_installed/ by default to save time).

.PARAMETER CleanAll
    When used with -Clean, also remove the vcpkg_installed/ cache. Default is $false.

.PARAMETER NoBuild
    Only configure CMake, do not build.

.PARAMETER NoTest
    Skip running tests after build.

.PARAMETER Configuration
    Build configuration(s): Debug, Release, or Both. Default is Debug.

.PARAMETER Generator
    CMake generator to use. Default is 'Visual Studio 17 2022'.

.PARAMETER BuildDir
    Build/output directory (relative to the script root or absolute).
    Default is 'build_vs'.

.PARAMETER CxxStandard
    C++ standard passed to CMAKE_CXX_STANDARD. Default is '23'.

.PARAMETER BuildTests
    Build the test suite (FASTRULES_BUILD_TESTS). Default is $true.

.PARAMETER BuildStressTests
    Build the stress tests (FASTRULES_BUILD_STRESS_TESTS). Default is $true.

.PARAMETER BuildExamples
    Build the examples (FASTRULES_BUILD_EXAMPLES). Default is $true.

.PARAMETER BuildExtensions
    Build the JSON/XML/DB extensions (FASTRULES_BUILD_EXTENSIONS). Default is $true.

.PARAMETER BuildShared
    Build a shared library instead of static (FASTRULES_BUILD_SHARED). Default is $false.

.PARAMETER BuildCoverage
    Enable coverage instrumentation (FASTRULES_BUILD_COVERAGE). Default is $false.

.PARAMETER VcpkgFeatures
    vcpkg manifest features to enable (VCPKG_MANIFEST_FEATURES).
    Default is tests, json, xml, db.

.PARAMETER ExtraCmakeArgs
    Additional raw arguments appended verbatim to the cmake configure command.

.EXAMPLE
    .\build.ps1
    Configure and build Debug.

.EXAMPLE
    .\build.ps1 -Configuration Release -NoTest
    Build Release without running tests.

.EXAMPLE
    .\build.ps1 -Configuration Both
    Build both Debug and Release.

.EXAMPLE
    .\build.ps1 -BuildExtensions:$false -VcpkgFeatures tests -BuildStressTests:$false
    Build a leaner configuration without extensions or stress tests.

.EXAMPLE
    .\build.ps1 -Generator 'Ninja Multi-Config' -BuildDir build_ninja
    Use a different generator and output directory.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$CleanAll,
    [switch]$NoBuild,
    [switch]$NoTest,
    [ValidateSet('Debug', 'Release', 'Both')]
    [string]$Configuration = 'Debug',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$BuildDir = 'build_vs',
    [string]$CxxStandard = '23',
    [bool]$BuildTests = $false,
    [bool]$BuildStressTests = $false,
    [bool]$BuildExamples = $false,
    [bool]$BuildExtensions = $false,
    [bool]$BuildShared = $false,
    [bool]$BuildCoverage = $false,
    [string[]]$VcpkgFeatures = @('tests', 'json', 'xml', 'db'),
    [string[]]$ExtraCmakeArgs = @()
)

# Helper: CMake wants ON/OFF, PowerShell gives us $true/$false.
function ConvertTo-CMakeBool([bool]$value) { if ($value) { 'ON' } else { 'OFF' } }

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $root) { $root = Get-Location }

# Resolve the build directory (honor absolute paths, otherwise relative to root).
if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $buildDir = $BuildDir
} else {
    $buildDir = Join-Path $root $BuildDir
}

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
    if (Test-Path $buildDir) {
        if ($CleanAll) {
            Remove-Item $buildDir -Recurse -Force
            Write-Host "  Removed: $buildDir (including vcpkg_installed)"
        } else {
            # Remove all contents except vcpkg_installed to preserve package cache.
            $vcpkgPath = Join-Path $buildDir 'vcpkg_installed'
            $hasVcpkg = Test-Path $vcpkgPath

            if ($hasVcpkg) {
                # Stash vcpkg_installed out of the way.
                $tempVcpkg = Join-Path ([System.IO.Path]::GetTempPath()) "vcpkg_installed_$([System.IO.Path]::GetRandomFileName())"
                Move-Item $vcpkgPath $tempVcpkg
                Write-Host "  Preserved vcpkg_installed to: $tempVcpkg"
            }

            # Remove the build directory.
            Remove-Item $buildDir -Recurse -Force
            Write-Host "  Removed: $buildDir (except vcpkg_installed)"

            # Restore vcpkg_installed if it was moved.
            if ($hasVcpkg) {
                $null = New-Item -ItemType Directory -Force $buildDir
                Move-Item $tempVcpkg $vcpkgPath
                Write-Host "  Restored vcpkg_installed"
            }
        }
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
Write-Host "  Generator:      $Generator"
Write-Host "  Build mode:     $Configuration"
Write-Host "  Build dir:      $buildDir"
Write-Host "  C++ Standard:   $CxxStandard"
Write-Host "  Tests:          $(ConvertTo-CMakeBool $BuildTests)"
Write-Host "  Stress:         $(ConvertTo-CMakeBool $BuildStressTests)"
Write-Host "  Examples:       $(ConvertTo-CMakeBool $BuildExamples)"
Write-Host "  Extensions:     $(ConvertTo-CMakeBool $BuildExtensions)"
Write-Host "  Shared:         $(ConvertTo-CMakeBool $BuildShared)"
Write-Host "  Coverage:       $(ConvertTo-CMakeBool $BuildCoverage)"
Write-Host ""

$cmakeArgs = @(
    '-B', $buildDir
    '-S', $root
    '-G', $Generator
    "-DCMAKE_CXX_STANDARD=$CxxStandard"
    '-DCMAKE_CXX_STANDARD_REQUIRED=ON'
    "-DFASTRULES_BUILD_STRESS_TESTS=$(ConvertTo-CMakeBool $BuildStressTests)"
    "-DFASTRULES_BUILD_TESTS=$(ConvertTo-CMakeBool $BuildTests)"
    "-DFASTRULES_BUILD_EXAMPLES=$(ConvertTo-CMakeBool $BuildExamples)"
    "-DFASTRULES_BUILD_EXTENSIONS=$(ConvertTo-CMakeBool $BuildExtensions)"
    "-DFASTRULES_BUILD_SHARED=$(ConvertTo-CMakeBool $BuildShared)"
    "-DFASTRULES_BUILD_COVERAGE=$(ConvertTo-CMakeBool $BuildCoverage)"
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
if ($VcpkgFeatures.Count -gt 0) {
    $cmakeArgs += "-DVCPKG_MANIFEST_FEATURES=$($VcpkgFeatures -join ';')"
}

# Append any caller-supplied raw cmake arguments last so they can override.
if ($ExtraCmakeArgs.Count -gt 0) {
    $cmakeArgs += $ExtraCmakeArgs
}

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
