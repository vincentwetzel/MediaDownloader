$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'build-debug'
$cachePath = Join-Path $buildRoot 'CMakeCache.txt'
$compilerMetadata = Get-ChildItem (Join-Path $buildRoot 'CMakeFiles') -Filter 'CMakeCXXCompiler.cmake' -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1

$needsFreshConfigure = $false
if (Test-Path -LiteralPath $cachePath) {
    $cacheText = Get-Content -LiteralPath $cachePath -Raw
    $generatorMatch = [regex]::Match($cacheText, '(?m)^CMAKE_GENERATOR:INTERNAL=(.+)$')
    if ($generatorMatch.Success -and $generatorMatch.Groups[1].Value.Trim() -ne 'Ninja') {
        $needsFreshConfigure = $true
        Write-Host "CMake generator mismatch detected ($($generatorMatch.Groups[1].Value.Trim())); using --fresh."
    }
}

if (-not $needsFreshConfigure -and -not $compilerMetadata) {
    $needsFreshConfigure = Test-Path -LiteralPath $cachePath
} else {
    if (-not $needsFreshConfigure) {
        $metadataText = Get-Content -LiteralPath $compilerMetadata.FullName -Raw
        $needsFreshConfigure =
            $metadataText -match 'set\(CMAKE_CXX_COMPILE_FEATURES\s*""\)' -or
            $metadataText -match 'set\(CMAKE_CXX_ABI_COMPILED\s*\)' -or
            $metadataText -match 'set\(CMAKE_CXX_COMPILER_WORKS\s*\)'
    }
}

if ($needsFreshConfigure) {
    Write-Host 'Incomplete CMake compiler metadata detected; using --fresh.'
    & cmake --fresh --preset debug
} else {
    & cmake --preset debug
}

exit $LASTEXITCODE
