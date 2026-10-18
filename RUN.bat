@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

REM ============================================================
REM BeanDraw 一键测试脚本（RUN.bat�?
REM   0. 工具链自动探测（Qt / CMake / Ninja / MSVC，常见安装位置）
REM   1. Rust 内核：build + test + clippy + fmt
REM   2. Qt UI：编�?+ windeployqt 收集依赖
REM   3. 无头冒烟测试：生成→统计→工程存取→导出 全链�?
REM   4. 全部通过则询问是否启�?BeanDraw
REM 用法：双击运行，或在项目根目录执�?RUN.bat
REM 环境要求：Rust 1.98 / Qt 6.7+（推�?6.12�? CMake 3.24+ / MSVC
REM ============================================================

set "ROOT=%~dp0"
set "RUST_DIR=%ROOT%rust_core"
set "UI_DIR=%ROOT%qt_ui"
set "BUILD_DIR=%ROOT%output\win"
set "EXE=%BUILD_DIR%\BeanDraw.exe"

set "FAILED=0"
set "HAVE_QT=0"

echo ============================================================
echo   BeanDraw 测试运行
echo   项目目录: %ROOT%
echo ============================================================
echo.

REM ---------- 0. 工具链自动探�?----------
call :DetectToolchain
if "%HAVE_QT%"=="1" (
    echo [工具链] Qt %QT_VER% ^(msvc^) + CMake + Ninja + MSVC 就绪
) else (
    echo [提示] 未检测到完整 Qt 工具链，将只运行 Rust 内核测试�?
    echo         需�? Qt 6.7+ ^(D:\QT �?C:\Qt^) + CMake 3.24+ + MSVC
)
echo.

REM ---------- 1. 检�?Rust 工具�?----------
where cargo >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找�?cargo，请先安�?Rust: https://rustup.rs
    echo         （版本要求见 rust-toolchain.toml: 1.98.1�?
    goto :END
)

echo ---------- [1/4] Rust 内核: cargo build ----------
pushd "%RUST_DIR%"
cargo build
if errorlevel 1 (
    echo [失败] cargo build 出错
    set "FAILED=1"
    popd
    goto :END
)

echo.
echo ---------- [2/4] Rust 内核: cargo test ----------
cargo test
if errorlevel 1 (
    echo [失败] cargo test 未通过
    set "FAILED=1"
    popd
    goto :END
)

echo.
echo ---------- [3/4] Rust 内核: clippy + fmt ----------
cargo clippy --all-targets -- -D warnings
if errorlevel 1 (
    echo [失败] clippy 有警�?错误（策�? -D warnings�?
    set "FAILED=1"
)
cargo fmt --check
if errorlevel 1 (
    echo [提示] 格式不符�?rustfmt，可执行: cd rust_core ^&^& cargo fmt
    set "FAILED=1"
)
popd
if "%FAILED%"=="1" goto :END

REM ---------- 2. Qt UI 构建 ----------
echo.
echo ---------- [4/4] Qt UI 构建 ----------
if "%HAVE_QT%"=="0" goto :UI_DONE

echo 正在配置并编�?Qt UI ^(首次约需数分钟^)...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM 工具链入 PATH（Qt/CMake/Ninja �?MSVC 之前，vcvars 最后调用以注入 cl�?
set "PATH=%QT_BIN%;%CMAKE_BIN%;%NINJA_BIN%;%PATH%"
if defined VCVARS call "%VCVARS%" >nul

cmake -S "%UI_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%"
if errorlevel 1 (
    echo [失败] cmake 配置失败。常见原�?
    echo         - ADS FetchContent 需要网络访�?github.com（首次配置）
    echo         - Qt 版本过低: 需�?6.7+
    set "FAILED=1"
    goto :UI_DONE
)
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 (
    echo [失败] Qt UI 编译失败，请检查上方错误输出�?
    set "FAILED=1"
    goto :UI_DONE
)
echo [完成] Qt UI 构建成功: %EXE%

REM ---------- 3. 收集 Qt 运行时依赖（否则启动会报缺少 Qt6*.dll�?----------
if exist "%EXE%" (
    where windeployqt >nul 2>nul
    if not errorlevel 1 (
        echo 正在收集 Qt 运行时依�?^(windeployqt^)...
        windeployqt --release --no-translations "%EXE%" >nul
    ) else (
        echo [提示] 未找�?windeployqt，若启动报缺�?Qt6*.dll 请把 Qt bin 加入 PATH�?
    )
)

REM ---------- 4. 无头冒烟测试：生成→统计→工程存取→导出 全链�?----------
if exist "%EXE%" (
    echo.
    echo ---------- 冒烟测试 ^(BeanDraw --smoke^) ----------
    "%EXE%" --smoke
    if errorlevel 1 (
        echo [失败] 冒烟测试未通过
        set "FAILED=1"
    ) else (
        echo [完成] 冒烟测试通过 ^(工程文件 / PDF / CSV 已生成于临时目录^)
    )
)

:UI_DONE
echo.

REM ---------- 5. 结果汇�?----------
echo ============================================================
if "%FAILED%"=="0" (
    echo   全部通过 �?
    echo     - Rust 内核: build / test / clippy / fmt 全过
    if "%HAVE_QT%"=="1" (
        echo     - Qt UI: 构建成功 + 冒烟测试通过
    ) else (
        echo     - Qt UI: 未构�?^(缺少 Qt/CMake 环境，界面不会弹出^)
    )
) else (
    echo   存在失败项，请检查上方输�?�?
)
echo ============================================================

REM ---------- 6. 直接启动（不再询问；先关闭旧实例，避免看到旧窗口�?----------
REM CM_NO_LAUNCH=1 时跳过自动启动（CI / 无显示环境）
if not defined CM_NO_LAUNCH if "%FAILED%"=="0" if exist "%EXE%" (
    echo.
    taskkill /F /IM BeanDraw.exe >nul 2>nul
    timeout /t 1 /nobreak >nul
    echo 正在启动 BeanDraw...
    start "" "%EXE%"
)

:END
echo.
REM 自动化场景（CI / 脚本调用）设 CM_NO_PAUSE=1 可跳过暂�?
if not defined CM_NO_PAUSE pause
endlocal
exit /b %FAILED%

REM ============================================================
REM 子程序：工具链探�?
REM   产出: HAVE_QT / QT_BIN / QT_ROOT / QT_VER / CMAKE_BIN / NINJA_BIN / VCVARS
REM ============================================================
:DetectToolchain
set "HAVE_QT=0"
set "QT_BIN="
set "QT_ROOT="
set "QT_VER="
set "CMAKE_BIN="
set "NINJA_BIN="
set "VCVARS="

REM --- CMake（Qt 安装目录自带优先，其�?PATH�?---
set "HAVE_CMAKE=0"
for %%D in (
  "D:\QT\Tools\CMake_64\bin"
  "C:\Qt\Tools\CMake_64\bin"
  "D:\QT\Tools\CMake\bin"
  "C:\Qt\Tools\CMake\bin"
) do (
  if exist %%D\cmake.exe (
    if not defined CMAKE_BIN set "CMAKE_BIN=%%~D"
    set "HAVE_CMAKE=1"
  )
)
where cmake >nul 2>nul
if not errorlevel 1 set "HAVE_CMAKE=1"

REM --- Qt bin（优先高版本；D:\QT �?C:\Qt 均探测） ---
for %%V in (6.12.0 6.11.2 6.10.0 6.9.0 6.8.0 6.7.0) do (
  for %%R in ("D:\QT\%%V\msvc2022_64" "C:\Qt\%%V\msvc2022_64" "D:\QT\%%V\msvc2019_64" "C:\Qt\%%V\msvc2019_64") do (
    if exist %%R\bin\qmake.exe (
      if not defined QT_BIN (
        set "QT_BIN=%%~R\bin"
        set "QT_ROOT=%%~R"
        set "QT_VER=%%V"
      )
    )
  )
)

REM --- Ninja ---
if exist "D:\QT\Tools\Ninja\ninja.exe" set "NINJA_BIN=D:\QT\Tools\Ninja"
if exist "C:\Qt\Tools\Ninja\ninja.exe" set "NINJA_BIN=C:\Qt\Tools\Ninja"

REM --- MSVC vcvars64.bat（经 vswhere 定位，兼�?VS 安装到非默认盘） ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -products * -property installationPath 2^>nul`) do (
    if not defined VS_PATH set "VS_PATH=%%i"
  )
)
if defined VS_PATH (
  if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
  for %%P in (
    "E:\VS\VStudio\VC\Auxiliary\Build\vcvars64.bat"
    "D:\VS\VStudio\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  ) do if exist %%P if not defined VCVARS set "VCVARS=%%~P"
)

REM --- 汇总判�?---
if defined QT_BIN if "%HAVE_CMAKE%"=="1" (
  set "HAVE_QT=1"
  if not defined VCVARS echo [提示] 未找�?vcvars64.bat，若 cl 不可用请先运�?VS 开发者命令提示符�?
) else (
  if not defined QT_BIN echo [提示] 未找�?Qt ^(qmake^)�?
  if "%HAVE_CMAKE%"=="0" echo [提示] 未找�?CMake�?
)
goto :EOF
