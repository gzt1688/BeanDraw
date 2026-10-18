@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM 打包_安装版.bat —— BeanDraw 安装版（Inno Setup）打包脚本
REM   1. 从 output\win 收集 exe + Qt 运行库 + 插件 + 翻译（暂存目录）
REM   2. 自动生成 BeanDraw.iss 安装脚本（含 VC++ 运行库静默安装）
REM   3. 调用 Inno Setup 编译出 output\pack\BeanDraw-<版本>-setup.exe
REM 前置条件：已用 RUN.bat 构建过（output\win\BeanDraw.exe 存在）
REM 可选：设 CM_ISCC 指向 ISCC.exe 使用自定义 Inno Setup
REM       CM_NO_PAUSE=1 跳过结尾暂停（自动化用）
REM ============================================================

set "ROOT=%~dp0"
set "SRC=%ROOT%output\win"
set "PACK_DIR=%ROOT%output\pack"
set "STAGE=%PACK_DIR%\setup-src"
set "ISS=%PACK_DIR%\BeanDraw.iss"
set "APP_VER=0.1.0"
set "ICON=%ROOT%qt_ui\assets\icons\favicon.ico"

if not exist "%SRC%\BeanDraw.exe" (
    echo [错误] 未找到 %SRC%\BeanDraw.exe
    echo        请先运行 RUN.bat 完成构建，再执行本打包脚本。
    goto :END
)

echo ============================================================
echo   BeanDraw 安装版打包  v%APP_VER%
echo   输出: %PACK_DIR%\BeanDraw-%APP_VER%-setup.exe
echo ============================================================
echo.

REM ============================================================
REM [A/3] 定位 Inno Setup 编译器 ISCC.exe
REM ============================================================
set "ISCC=%CM_ISCC%"
for %%P in (
  "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
  "%ProgramFiles%\Inno Setup 6\ISCC.exe"
  "D:\Inno Setup 6\ISCC.exe"
  "D:\Program Files\Inno Setup 6\ISCC.exe"
  "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
) do if not defined ISCC if exist %%P set "ISCC=%%~P"
if defined ISCC goto :ISCC_OK

echo [提示] 未找到 Inno Setup 6（ISCC.exe），安装版编译需要它。
where winget >nul 2>nul
if errorlevel 1 goto :NEED_ISCC
choice /c YN /m "是否现在用 winget 自动安装 Inno Setup（J R Software）?"
if errorlevel 2 goto :NEED_ISCC
echo 正在安装 Inno Setup ...
winget install --id JRSoftware.InnoSetup -e --silent --accept-package-agreements --accept-source-agreements
if errorlevel 1 goto :NEED_ISCC
for %%P in (
  "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
  "%ProgramFiles%\Inno Setup 6\ISCC.exe"
) do if not defined ISCC if exist %%P set "ISCC=%%~P"
if not defined ISCC goto :NEED_ISCC

:ISCC_OK
echo [工具] Inno Setup: %ISCC%
echo.

REM ============================================================
REM [B/3] 暂存目录：复制 exe / dll / qm / 插件
REM ============================================================
echo [1/2] 收集安装文件到暂存目录...
if exist "%STAGE%" rd /s /q "%STAGE%"
mkdir "%STAGE%" 2>nul

copy /y "%SRC%\BeanDraw.exe" "%STAGE%\" >nul
copy /y "%SRC%\Qt6Core.dll" "%STAGE%\" >nul
copy /y "%SRC%\Qt6Gui.dll" "%STAGE%\" >nul
copy /y "%SRC%\Qt6Widgets.dll" "%STAGE%\" >nul
copy /y "%SRC%\Qt6Network.dll" "%STAGE%\" >nul
copy /y "%SRC%\Qt6PrintSupport.dll" "%STAGE%\" >nul
copy /y "%SRC%\Qt6Svg.dll" "%STAGE%\" >nul
copy /y "%SRC%\opengl32sw.dll" "%STAGE%\" >nul
copy /y "%SRC%\d3dcompiler_47.dll" "%STAGE%\" >nul
copy /y "%SRC%\dxcompiler.dll" "%STAGE%\" >nul
copy /y "%SRC%\dxil.dll" "%STAGE%\" >nul
copy /y "%SRC%\beandraw_zh_CN.qm" "%STAGE%\" >nul
copy /y "%SRC%\beandraw_en_US.qm" "%STAGE%\" >nul
copy /y "%SRC%\vc_redist.x64.exe" "%STAGE%\" >nul
if errorlevel 1 (
    echo [错误] 复制核心文件失败，请确认 output\win 是完整构建产物。
    goto :END
)
for %%D in (platforms styles iconengines imageformats tls networkinformation translations) do (
    if exist "%SRC%\%%D" (
        mkdir "%STAGE%\%%D" 2>nul
        copy /y "%SRC%\%%D\*.*" "%STAGE%\%%D\" >nul
    )
)

REM ============================================================
REM [C/3] 生成 Inno Setup 脚本（含中文向导自动部署）
REM ============================================================
echo [2/2] 生成 Inno Setup 脚本...
if exist "%ISS%" del /q "%ISS%"

REM 有官方简中翻译时启用中文向导，否则默认英文
for %%P in ("%ISCC%") do set "ISCC_DIR=%%~dpP"
echo [Languages]>>"%ISS%"
REM 项目内置简中语言文件，Inno 目录缺失时自动部署（仓库随带，离线可用）
if exist "%ROOT%pack\ChineseSimplified.isl" if not exist "%ISCC_DIR%Languages\ChineseSimplified.isl" (
    copy /y "%ROOT%pack\ChineseSimplified.isl" "%ISCC_DIR%Languages\ChineseSimplified.isl" >nul
)
if exist "%ISCC_DIR%Languages\ChineseSimplified.isl" goto :HAS_ZH
echo Name: "en"; MessagesFile: "compiler:Default.isl" >>"%ISS%"
goto :LANG_DONE
:HAS_ZH
echo Name: "chs"; MessagesFile: "compiler:Languages\ChineseSimplified.isl" >>"%ISS%"
:LANG_DONE

(
echo.
echo [Setup]
echo AppId={{7C1D98E2-6B3A-4F1A-9D5E-3A2C8B4E5F60}
echo AppName=BeanDraw
echo AppVersion=%APP_VER%
echo AppVerName=BeanDraw %APP_VER%
echo AppPublisher=BeanDraw
echo DefaultDirName={autopf}\BeanDraw
echo DefaultGroupName=BeanDraw
echo DisableProgramGroupPage=yes
echo OutputDir="%PACK_DIR%"
echo OutputBaseFilename=BeanDraw-%APP_VER%-setup
echo SetupIconFile="%ICON%"
echo UninstallDisplayIcon={app}\BeanDraw.exe
echo Compression=lzma2/max
echo SolidCompression=yes
echo WizardStyle=modern
echo ArchitecturesInstallIn64BitMode=x64compatible
echo ArchitecturesAllowed=x64compatible
echo PrivilegesRequiredOverridesAllowed=dialog commandline
echo.
echo [Tasks]
echo Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
echo.
echo [Files]
echo Source: "%STAGE%\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
echo Source: "%STAGE%\BeanDraw.exe"; DestDir: "{app}"; Flags: ignoreversion
echo Source: "%STAGE%\*.dll"; DestDir: "{app}"; Flags: ignoreversion
echo Source: "%STAGE%\*.qm"; DestDir: "{app}"; Flags: ignoreversion
)>>"%ISS%"
REM 插件目录按实际存在生成安装条目（windeployqt 未生成的目录自动跳过）
for %%D in (platforms styles iconengines imageformats tls networkinformation translations) do (
    if exist "%STAGE%\%%D" echo Source: "%STAGE%\%%D\*"; DestDir: "{app}\%%D"; Flags: ignoreversion recursesubdirs createallsubdirs>>"%ISS%"
)
(
echo.
echo [Icons]
echo Name: "{autoprograms}\BeanDraw"; Filename: "{app}\BeanDraw.exe"
echo Name: "{autodesktop}\BeanDraw"; Filename: "{app}\BeanDraw.exe"; Tasks: desktopicon
echo.
echo [Run]
echo Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "正在安装 VC++ 运行库（已装则自动跳过）..."; Flags: waituntilterminated
echo Filename: "{app}\BeanDraw.exe"; Description: "{cm:LaunchProgram,BeanDraw}"; Flags: nowait postinstall skipifsilent
echo.
echo [UninstallDelete]
echo Type: filesandordirs; Name: "{app}\platforms"
echo Type: filesandordirs; Name: "{app}\styles"
echo Type: filesandordirs; Name: "{app}\iconengines"
echo Type: filesandordirs; Name: "{app}\imageformats"
echo Type: filesandordirs; Name: "{app}\tls"
echo Type: filesandordirs; Name: "{app}\networkinformation"
echo Type: filesandordirs; Name: "{app}\translations"
)>>"%ISS%"

echo.
echo [编译] 正在编译安装包（lzma2 压缩，约需一两分钟）...
"%ISCC%" /Qp "%ISS%"
if errorlevel 1 (
    echo [错误] Inno Setup 编译失败，请查看上方 ISCC 输出定位问题行。
    echo        iss 脚本: %ISS%（可手工微调后重试）
    goto :END
)

echo.
echo ============================================================
echo   打包完成 √
echo     安装包: %PACK_DIR%\BeanDraw-%APP_VER%-setup.exe
echo     iss 脚本: %ISS%
echo ============================================================

:END
echo.
if not defined CM_NO_PAUSE pause
endlocal
exit /b 0

:NEED_ISCC
echo [错误] 未能获取 ISCC.exe，无法编译安装包。
echo        解决办法任选其一：
echo          1. 重新运行本脚本并选择自动安装 Inno Setup
echo          2. 手动安装: https://jrsoftware.org/isdl.php （默认路径即可）
echo          3. 设环境变量 CM_ISCC 指向 ISCC.exe 后再运行本脚本
goto :END
