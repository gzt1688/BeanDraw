@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM 打包_绿色版.bat —— BeanDraw 便携版（免安装）打包脚本
REM   1. 从 output\win 收集 exe + Qt 运行库 + 插件 + 翻译
REM   2. 生成“使用说明.txt”
REM   3. 压缩为 output\pack\BeanDraw-<版本>-portable.zip
REM 前置条件：已用 RUN.bat 构建过（output\win\BeanDraw.exe 存在）
REM 可选：CM_NO_PAUSE=1 跳过结尾暂停（自动化用）
REM ============================================================

set "ROOT=%~dp0"
set "SRC=%ROOT%output\win"
set "PACK_DIR=%ROOT%output\pack"
set "APP_VER=0.1.0"
set "STAGE=%PACK_DIR%\BeanDraw-%APP_VER%-portable"

if not exist "%SRC%\BeanDraw.exe" (
    echo [错误] 未找到 %SRC%\BeanDraw.exe
    echo        请先运行 RUN.bat 完成构建，再执行本打包脚本。
    goto :END
)

echo ============================================================
echo   BeanDraw 绿色版打包  v%APP_VER%
echo   输出: %STAGE%
echo ============================================================
echo.

REM ---------- 清理旧的暂存目录 ----------
if exist "%STAGE%" rd /s /q "%STAGE%"
mkdir "%STAGE%" 2>nul

echo [1/4] 复制程序与运行库...
copy /y "%SRC%\BeanDraw.exe"              "%STAGE%\" >nul
copy /y "%SRC%\Qt6Core.dll"                  "%STAGE%\" >nul
copy /y "%SRC%\Qt6Gui.dll"                   "%STAGE%\" >nul
copy /y "%SRC%\Qt6Widgets.dll"               "%STAGE%\" >nul
copy /y "%SRC%\Qt6Network.dll"               "%STAGE%\" >nul
copy /y "%SRC%\Qt6PrintSupport.dll"          "%STAGE%\" >nul
copy /y "%SRC%\Qt6Svg.dll"                   "%STAGE%\" >nul
copy /y "%SRC%\opengl32sw.dll"               "%STAGE%\" >nul
copy /y "%SRC%\d3dcompiler_47.dll"           "%STAGE%\" >nul
copy /y "%SRC%\dxcompiler.dll"               "%STAGE%\" >nul
copy /y "%SRC%\dxil.dll"                     "%STAGE%\" >nul
if errorlevel 1 (
    echo [错误] 复制核心文件失败，请确认 output\win 是完整构建产物。
    goto :END
)

REM 语言包（界面中/英文翻译，与 exe 同目录）
copy /y "%SRC%\beandraw_zh_CN.qm" "%STAGE%\" >nul
copy /y "%SRC%\beandraw_en_US.qm" "%STAGE%\" >nul

REM Qt 插件目录结构必须保持原样
for %%D in (platforms styles iconengines imageformats tls networkinformation translations) do (
    if exist "%SRC%\%%D" (
        mkdir "%STAGE%\%%D" 2>nul
        copy /y "%SRC%\%%D\*.*" "%STAGE%\%%D\" >nul
    )
)

REM 附带 VC++ 运行库安装器（目标机器缺运行库时手动安装一次）
copy /y "%SRC%\vc_redist.x64.exe" "%STAGE%\vc_redist.x64.exe" >nul

echo [2/4] 生成使用说明...
> "%STAGE%\使用说明.txt" (
    echo BeanDraw v%APP_VER% 绿色版
    echo ----------------------------------------
    echo 免安装便携版：解压到任意目录后直接双击 BeanDraw.exe 运行。
    echo.
    echo 注意：
    echo  1. 请勿单独移动或删除 exe 同目录下的 dll / 插件文件夹。
    echo  2. 若提示缺少 VCRUNTIME140.dll 等运行库，请安装附带的
    echo     vc_redist.x64.exe（一次性，装过可跳过）。
    echo  3. 程序设置保存在系统注册表（HKCU\Software\BeanDraw），
    echo     工程文件请自行保存到经常使用的目录。
)

echo [3/4] 冒烟测试（--smoke，验证包内运行链路）...
pushd "%STAGE%"
"%STAGE%\BeanDraw.exe" --smoke >nul 2>&1
set "SMOKE_RC=%errorlevel%"
popd
if not "%SMOKE_RC%"=="0" (
    echo [警告] 冒烟测试未通过（退出码 %SMOKE_RC%），仍会继续打包，
    echo        但建议先回到开发目录运行 RUN.bat 检查。
) else (
    echo [完成] 冒烟测试通过。
)

echo [4/4] 压缩为 zip...
if exist "%PACK_DIR%\BeanDraw-%APP_VER%-portable.zip" del /q "%PACK_DIR%\BeanDraw-%APP_VER%-portable.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%PACK_DIR%\BeanDraw-%APP_VER%-portable.zip' -Force"
if errorlevel 1 (
    echo [错误] zip 压缩失败（PowerShell Compress-Archive）。
    echo        未压缩的完整目录仍可用：%STAGE%
    goto :END
)

echo.
echo ============================================================
echo   打包完成 √
echo     便携目录: %STAGE%
echo     压缩包  : %PACK_DIR%\BeanDraw-%APP_VER%-portable.zip
echo ============================================================

:END
echo.
if not defined CM_NO_PAUSE pause
endlocal
exit /b 0
