param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$OpenSSLRoot = '',
    [string]$BuildDirectory = '',
    [switch]$WithoutOptionalModules
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build/windows/$Configuration"
if ($BuildDirectory) { $build = [IO.Path]::GetFullPath($BuildDirectory) }
$optional = if ($WithoutOptionalModules) { 'OFF' } else { 'ON' }
$compiler = (Get-Command cl -ErrorAction Stop).Source

# Run from an x64 Visual Studio developer shell with CMake and Ninja on PATH.
$configure = @(
    '-S', $root, '-B', $build, '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_CXX_COMPILER=$compiler",
    '-DKITZOO_BUILD_TESTS=ON', '-DKITZOO_BUILD_BENCHMARKS=ON',
    '-DKITZOO_BUILD_EXAMPLES=ON', '-DKITZOO_WARNINGS_AS_ERRORS=ON',
    '-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER',
    "-DKITZOO_WITH_OPENSSL=$optional", "-DKITZOO_WITH_MIMALLOC=$optional"
)
if ($OpenSSLRoot) {
    $configure += "-DOPENSSL_ROOT_DIR=$OpenSSLRoot"
    # OpenSSL DLLs must also be available when discovering/running tests.
    $env:PATH = (Join-Path $OpenSSLRoot 'bin') + ';' + $env:PATH
}
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& cmake --build $build --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& ctest --test-dir $build --output-on-failure --no-tests=error --timeout 300
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
