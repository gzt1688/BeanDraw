# BeanDraw 翻译更新脚本（开发大纲 build_scripts/translate_update.ps1）
# 用法：powershell -File translate_update.ps1
# 作用：lupdate 提取源码 tr() 文本 → 更新 ts → lrelease 生成 qm

param(
    [string]$QtDir = $env:QT_DIR
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$uiDir = Join-Path $root "qt_ui"
$tsDir = Join-Path $uiDir "src/ui/i18n/translations"

$lupdate = if ($QtDir) { Join-Path $QtDir "bin/lupdate.exe" } else { "lupdate" }
$lrelease = if ($QtDir) { Join-Path $QtDir "bin/lrelease.exe" } else { "lrelease" }

$tsFiles = Get-ChildItem $tsDir -Filter "*.ts" | ForEach-Object { $_.FullName }
if (-not $tsFiles) { throw "no ts files found in $tsDir" }

Write-Host "==> lupdate: extracting tr() strings"
& $lupdate $uiDir/src -ts $tsFiles -no-obsolete
if ($LASTEXITCODE -ne 0) { throw "lupdate failed" }

Write-Host "==> lrelease: compiling qm"
& $lrelease $tsFiles
if ($LASTEXITCODE -ne 0) { throw "lrelease failed" }

Write-Host "==> Done. Review ts changes and commit."
