# One-shot build (Windows / MinGW). ASCII only on purpose (PS 5.1 decodes BOM-less UTF-8 as GBK).
#
#   Two paths:
#     [default] direct g++ -- reliable everywhere, no CMake needed
#     -CMake        drive CMake + Ninja instead (the path an outside user would take;
#                   needs cmake on PATH, which the local toolchain provides)
#
#   Adds the local toolchain to PATH **for this process only** (does not touch system env).
#
#   Usage:
#     powershell -ExecutionPolicy Bypass -File build.ps1
#     powershell -ExecutionPolicy Bypass -File build.ps1 -Clean -Test
#     powershell -ExecutionPolicy Bypass -File build.ps1 -CMake
param(
    [switch]$Clean,
    [switch]$Test,
    [switch]$CMake,
    [string]$BuildDir = 'build'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

$kit = Join-Path $root '..\.util\toolchain\w64devkit\bin'
if (-not (Test-Path $kit)) {
    Write-Error "toolchain not found: $kit"
    exit 2
}
$env:PATH = "$kit;$env:PATH"

$srcs = @('src\main.cpp', 'src\engine.cpp', 'src\tables.cpp', 'src\hqreport.cpp', 'src\csv.cpp', 'src\solver.cpp')
$out  = Join-Path $root 'craftweave.exe'
$testOut = Join-Path $root 'tests\test_core.exe'

if ($CMake) {
    $bd = Join-Path $root $BuildDir
    if ($Clean -and (Test-Path $bd)) { Remove-Item -Recurse -Force $bd }
    Write-Host "[1/2] configure -> $bd"
    cmake -S $root -B $bd -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "[2/2] build"
    cmake --build $bd
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $out = Join-Path $bd 'craftweave.exe'
    $testOut = Join-Path $bd 'test_core.exe'
} else {
    Write-Host "[1/2] build craftweave.exe (direct g++)"
    & g++ -O2 -std=c++17 -Iinclude -o $out @srcs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "[2/2] build tests\test_core.exe"
    & g++ -O2 -std=c++17 -Iinclude -o $testOut 'tests\test_main.cpp' 'src\engine.cpp' 'src\tables.cpp' 'src\hqreport.cpp' 'src\csv.cpp' 'src\solver.cpp'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host ""
Write-Host "OK -> $out"
if ($Test) {
    Write-Host ""
    Write-Host "[test] run engine tests"
    $env:WEAVER_DATA = Join-Path $root 'data'
    & $testOut
    exit $LASTEXITCODE
}
