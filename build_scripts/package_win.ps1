# BeanDraw Windows 打包脚本（开发大纲 build_scripts/package_win.ps1）
# 依赖 build_win.ps1 产物：output/win/BeanDraw.exe
# 产出：output/win/pack/BeanDraw-<version>-win64.7z（绿色免安装包）

param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "output/win"
$packDir = Join-Path $buildDir "pack/BeanDraw"

$exe = Join-Path $buildDir "BeanDraw.exe"
if (-not (Test-Path $exe)) { throw "BeanDraw.exe not found, run build_win.ps1 first" }

Write-Host "==> Collecting Qt dependencies (windeployqt)"
$windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue
if ($windeployqt) {
    & $windeployqt.Source --release --no-translations $exe
    if ($LASTEXITCODE -ne 0) { Write-Warning "windeployqt reported errors" }
} else {
    Write-Warning "windeployqt not found; package may be incomplete"
}

Write-Host "==> Assembling green package"
if (Test-Path $packDir) { Remove-Item -Recurse -Force $packDir }
New-Item -ItemType Directory -Force -Path $packDir | Out-Null

# exe + 依赖 + 翻译 + Rust 内核 DLL（cdylib 形态时）
Get-ChildItem $buildDir -File | Where-Object {
    $_.Extension -in ".exe", ".dll"
} | Copy-Item -Destination $packDir

if (Test-Path (Join-Path $buildDir "translations")) {
    Copy-Item -Recurse (Join-Path $buildDir "translations") $packDir
}

$version = "0.1.0"
$archive = Join-Path $buildDir "pack/BeanDraw-$version-win64.7z"
Write-Host "==> Creating archive $archive"
& 7z a -t7z -mx=5 $archive (Join-Path $packDir "*") | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Warning "7z not available; skip archiving" }

Write-Host "==> Done: $packDir"
