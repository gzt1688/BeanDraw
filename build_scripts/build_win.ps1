# BeanDraw Windows 一键构建脚本（开发大纲 build_scripts）
# 用法：powershell -File build_win.ps1 [-Config Debug|Release] [-SkipRust]

param(
    [string]$Config = "Release",
    [switch]$SkipRust
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rustDir = Join-Path $root "rust_core"
$uiDir = Join-Path $root "qt_ui"
$buildDir = Join-Path $root "output/win"

Write-Host "==> [1/3] Building rust_core ($Config)"
if (-not $SkipRust) {
    $cargoArgs = @("build", "--manifest-path", (Join-Path $rustDir "Cargo.toml"))
    if ($Config -eq "Release") { $cargoArgs += "--release" }
    & cargo @cargoArgs
    if ($LASTEXITCODE -ne 0) { throw "cargo build failed" }
}

Write-Host "==> [2/3] Configuring Qt project"
$cargoProfile = if ($Config -eq "Debug") { "debug" } else { "release" }
$rustLib = Join-Path $rustDir "target/$cargoProfile/craftmaster_core.lib"
if (-not (Test-Path $rustLib)) {
    throw "Rust static library not found: $rustLib"
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
cmake -S $uiDir -B $buildDir -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "==> [3/3] Building Qt UI"
cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

Write-Host "==> Done. Artifacts in $buildDir"
