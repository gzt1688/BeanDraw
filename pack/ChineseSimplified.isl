; *** Inno Setup 简体中文消息 ***（基于 6.5+ Default.isl 翻译）
; 注：勿给原本没有句号的消息补句号（Inno 会自动追加）。

[LangOptions]
LanguageName=简体中文
LanguageID=$0804
LanguageCodePage=936
;DialogFontName=
;DialogFontSize=9

[Messages]

; *** 应用标题
SetupAppTitle=安装程序
SetupWindowTitle=安装 - %1
UninstallAppTitle=卸载
UninstallAppFullTitle=%1 卸载

; *** 常用杂项
InformationTitle=信息
ConfirmTitle=确认
ErrorTitle=错误

; *** SetupLdr 消息
SetupLdrStartupMessage=这将安装 %1。是否继续？
LdrCannotCreateTemp=无法创建临时文件。安装已中止
LdrCannotExecTemp=无法在临时目录中执行文件。安装已中止
HelpTextNote=

; *** 启动错误消息
LastErrorMessage=%1。%n%n错误 %2: %3
SetupFileMissing=安装源目录中缺少文件 %1。请更正此问题或重新获取一份程序。
SetupFileCorrupt=安装文件已损坏。请重新获取一份程序。
SetupFileCorruptOrWrongVer=安装文件已损坏，或与当前安装程序版本不兼容。请更正此问题或重新获取一份程序。
InvalidParameter=命令行参数无效:%n%n%1
SetupAlreadyRunning=安装程序已在运行。
WindowsVersionNotSupported=本程序不支持你当前运行的 Windows 版本。
WindowsServicePackRequired=本程序需要 %1 Service Pack %2 或更高版本。
NotOnThisPlatform=本程序不能在 %1 上运行。
OnlyOnThisPlatform=本程序只能在 %1 上运行。
OnlyOnTheseArchitectures=本程序只能安装在为以下处理器架构设计的 Windows 版本上:%n%n%1
WinVersionTooLowError=本程序需要 %1 版本 %2 或更高版本。
WinVersionTooHighError=本程序不能安装在 %1 版本 %2 或更高版本上。
AdminPrivilegesRequired=安装本程序时必须以管理员身份登录。
PowerUserPrivilegesRequired=安装本程序时必须以管理员或 Power Users 组成员身份登录。
SetupAppRunningError=检测到 %1 正在运行。%n%n请关闭其所有实例，然后单击“确定”继续，或“取消”退出。
UninstallAppRunningError=检测到 %1 正在运行。%n%n请关闭其所有实例，然后单击“确定”继续，或“取消”退出。

; *** 启动选择
PrivilegesRequiredOverrideTitle=选择安装程序的安装模式
PrivilegesRequiredOverrideInstruction=选择安装模式
PrivilegesRequiredOverrideText1=%1 可以为所有用户安装（需要管理员权限），也可以只为当前用户安装。
PrivilegesRequiredOverrideText2=%1 可以只为当前用户安装，也可以为所有用户安装（需要管理员权限）。
PrivilegesRequiredOverrideAllUsers=为所有用户安装(&A)
PrivilegesRequiredOverrideAllUsersRecommended=为所有用户安装（推荐）(&A)
PrivilegesRequiredOverrideCurrentUser=仅为我安装(&M)
PrivilegesRequiredOverrideCurrentUserRecommended=仅为我安装（推荐）(&M)

; *** 其他错误
ErrorCreatingDir=安装程序无法创建目录“%1”
ErrorTooManyFilesInDir=无法在目录“%1”中创建文件：文件数过多

; *** 安装程序通用消息
ExitSetupTitle=退出安装
ExitSetupMessage=安装尚未完成。如果现在退出，程序将不会被安装。%n%n你可以稍后再次运行安装程序来完成安装。%n%n退出安装吗？
AboutSetupMenuItem=关于安装程序(&A)...
AboutSetupTitle=关于安装程序
AboutSetupMessage=%1 版本 %2%n%3%n%n%1 主页:%n%4
AboutSetupNote=
TranslatorNote=

; *** 按钮
ButtonBack=< 上一步(&B)
ButtonNext=下一步(&N) >
ButtonInstall=安装(&I)
ButtonOK=确定
ButtonCancel=取消
ButtonYes=是(&Y)
ButtonYesToAll=全部是(&A)
ButtonNo=否(&N)
ButtonNoToAll=全部否(&O)
ButtonFinish=完成(&F)
ButtonBrowse=浏览(&B)...
ButtonWizardBrowse=浏览(&R)...
ButtonNewFolder=新建文件夹(&M)

; *** “选择语言”对话框
SelectLanguageTitle=选择安装语言
SelectLanguageLabel=选择安装期间要使用的语言。

; *** 向导通用文本
ClickNext=单击“下一步”继续，或“取消”退出安装程序。
BeveledLabel=
BrowseDialogTitle=浏览文件夹
BrowseDialogLabel=在下面列表中选择一个文件夹，然后单击“确定”。
NewFolderName=新建文件夹

; *** “欢迎”向导页
WelcomeLabel1=欢迎使用 [name] 安装向导
WelcomeLabel2=这将在你的计算机上安装 [name/ver]。%n%n建议继续之前关闭所有其他应用程序。

; *** “密码”向导页
WizardPassword=密码
PasswordLabel1=此安装受密码保护。
PasswordLabel3=请输入密码，然后单击“下一步”继续。密码区分大小写。
PasswordEditLabel=密码(&P):
IncorrectPassword=输入的密码不正确。请重试。

; *** “许可协议”向导页
WizardLicense=许可协议
LicenseLabel=请在继续之前阅读以下重要信息。
LicenseLabel3=请阅读以下许可协议。必须接受本协议的条款才能继续安装。
LicenseAccepted=我接受协议(&A)
LicenseNotAccepted=我不接受协议(&D)

; *** “信息”向导页
WizardInfoBefore=信息
InfoBeforeLabel=请在继续之前阅读以下重要信息。
InfoBeforeClickLabel=准备好继续安装时，单击“下一步”。
WizardInfoAfter=信息
InfoAfterLabel=请在继续之前阅读以下重要信息。
InfoAfterClickLabel=准备好继续安装时，单击“下一步”。

; *** “用户信息”向导页
WizardUserInfo=用户信息
UserInfoDesc=请输入你的信息。
UserInfoName=用户名(&U):
UserInfoOrg=组织(&O):
UserInfoSerial=序列号(&S):
UserInfoNameRequired=必须输入用户名。

; *** “选择目标位置”向导页
WizardSelectDir=选择目标位置
SelectDirDesc=[name] 应安装到哪里？
SelectDirLabel3=安装程序将把 [name] 安装到以下文件夹。
SelectDirBrowseLabel=单击“下一步”继续。想到其他文件夹可单击“浏览”。
DiskSpaceGBLabel=需要至少 [gb] GB 的可用磁盘空间。
DiskSpaceMBLabel=需要至少 [mb] MB 的可用磁盘空间。
CannotInstallToNetworkDrive=安装程序无法安装到网络驱动器。
CannotInstallToUNCPath=安装程序无法安装到 UNC 路径。
InvalidPath=必须输入带盘号的完整路径，例如:%n%nC:\APP%n%n或形如以下的 UNC 路径:%n%n\\server\share
InvalidDrive=所选驱动器或 UNC 共享不存在或不可访问。请选择其他位置。
DiskSpaceWarningTitle=磁盘空间不足
DiskSpaceWarning=安装程序需要至少 %1 KB 的可用空间才能安装，但所选驱动器只有 %2 KB 可用。%n%n仍要继续吗？
DirNameTooLong=文件夹名称或路径过长。
InvalidDirName=文件夹名称无效。
BadDirName32=文件夹名称不能包含以下任何字符:%n%n%1
DirExistsTitle=文件夹已存在
DirExists=文件夹:%n%n%1%n%n已存在。仍要安装到该文件夹吗？
DirDoesntExistTitle=文件夹不存在
DirDoesntExist=文件夹:%n%n%1%n%n不存在。需要创建该文件夹吗？

; *** “选择组件”向导页
WizardSelectComponents=选择组件
SelectComponentsDesc=应安装哪些组件？
SelectComponentsLabel2=选择要安装的组件；取消选择不想安装的组件。准备好继续时单击“下一步”。
FullInstallation=完全安装
CompactInstallation=精简安装
CustomInstallation=自定义安装
NoUninstallWarningTitle=组件已存在
NoUninstallWarning=安装程序检测到以下组件已安装在计算机上:%n%n%1%n%n取消选择这些组件不会卸载它们。%n%n仍要继续吗？
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=当前选择需要至少 [gb] GB 磁盘空间。
ComponentsDiskSpaceMBLabel=当前选择需要至少 [mb] MB 磁盘空间。

; *** “选择附加任务”向导页
WizardSelectTasks=选择附加任务
SelectTasksDesc=应执行哪些附加任务？
SelectTasksLabel2=选择希望安装程序在安装 [name] 期间执行的附加任务，然后单击“下一步”。

; *** “选择开始菜单文件夹”向导页
WizardSelectProgramGroup=选择开始菜单文件夹
SelectStartMenuFolderDesc=安装程序应把程序快捷方式放在哪里？
SelectStartMenuFolderLabel3=安装程序将在以下开始菜单文件夹中创建程序快捷方式。
SelectStartMenuFolderBrowseLabel=单击“下一步”继续。想到其他文件夹可单击“浏览”。
MustEnterGroupName=必须输入文件夹名称。
GroupNameTooLong=文件夹名称或路径过长。
InvalidGroupName=文件夹名称无效。
BadGroupName=文件夹名称不能包含以下任何字符:%n%n%1
NoProgramGroupCheck2=不创建开始菜单文件夹(&D)

; *** “准备安装”向导页
WizardReady=准备安装
ReadyLabel1=安装程序已准备好在计算机上安装 [name]。
ReadyLabel2a=单击“安装”继续安装；若要查看或更改设置，请单击“上一步”。
ReadyLabel2b=单击“安装”继续安装。
ReadyMemoUserInfo=用户信息:
ReadyMemoDir=目标位置:
ReadyMemoType=安装类型:
ReadyMemoComponents=所选组件:
ReadyMemoGroup=开始菜单文件夹:
ReadyMemoTasks=附加任务:

; *** 下载向导页
DownloadingLabel2=正在下载文件...
ButtonStopDownload=停止下载(&S)
StopDownload=确定要停止下载吗？
ErrorDownloadAborted=下载已中止
ErrorDownloadFailed=下载失败: %1 %2
ErrorDownloadSizeFailed=获取大小失败: %1 %2
ErrorProgress=无效进度: %1 / %2
ErrorFileSize=无效文件大小: 预期 %1，实际 %2

; *** 解压向导页
ExtractingLabel=正在解压文件...
ButtonStopExtraction=停止解压(&S)
StopExtraction=确定要停止解压吗？
ErrorExtractionAborted=解压已中止
ErrorExtractionFailed=解压失败: %1

; *** 归档解压失败细节
ArchiveIncorrectPassword=密码不正确
ArchiveIsCorrupted=归档文件已损坏
ArchiveUnsupportedFormat=归档格式不受支持

; *** “准备安装”阶段
WizardPreparing=准备安装
PreparingDesc=安装程序正在准备在计算机上安装 [name]。
PreviousInstallNotCompleted=之前某个程序的安装/卸载未完成。需要重启计算机才能完成该安装。%n%n重启计算机后，请再次运行安装程序来完成 [name] 的安装。
CannotContinue=安装程序无法继续。请单击“取消”退出。
ApplicationsFound=以下应用程序正在使用需要由安装程序更新的文件。建议让安装程序自动关闭这些应用程序。
ApplicationsFound2=以下应用程序正在使用需要由安装程序更新的文件。建议让安装程序自动关闭这些应用程序。安装完成后，安装程序会尝试重新启动它们。
CloseApplications=自动关闭应用程序(&A)
DontCloseApplications=不关闭应用程序(&D)
ErrorCloseApplications=安装程序未能自动关闭所有应用程序。建议在继续之前手动关闭所有使用相关文件的应用程序。
PrepareToInstallNeedsRestart=安装程序必须重启计算机。重启后请再次运行安装程序来完成 [name] 的安装。%n%n现在重启吗？

; *** “正在安装”向导页
WizardInstalling=正在安装
InstallingLabel=请稍候，安装程序正在安装 [name]...

; *** “安装完成”向导页
FinishedHeadingLabel=[name] 安装向导即将完成
FinishedLabelNoIcons=安装程序已完成在计算机上安装 [name]。
FinishedLabel=安装程序已完成在计算机上安装 [name]。可以通过所选快捷方式运行本程序。
ClickFinish=单击“完成”退出安装程序。
FinishedRestartLabel=为完成 [name] 的安装，安装程序必须重启计算机。现在重启吗？
FinishedRestartMessage=为完成 [name] 的安装，安装程序必须重启计算机。%n%n现在重启吗？
ShowReadmeCheck=是，我想查看 README 文件
YesRadio=是，立即重启计算机(&Y)
NoRadio=否，稍后手动重启(&N)
RunEntryExec=运行 %1
RunEntryShellExec=查看 %1

; *** “需要下一张磁盘”
ChangeDiskTitle=安装程序需要下一张磁盘
SelectDiskLabel2=请插入磁盘 %1 后单击“确定”。%n%n如果此磁盘上的文件不在下面显示的文件夹中，请输入正确路径或单击“浏览”。
PathLabel=路径(&P):
FileNotInDir2=在“%2”中找不到文件“%1”。请插入正确的磁盘或选择其他文件夹。
SelectDirectoryLabel=请指定下一张磁盘的位置。

; *** 安装阶段消息
SetupAborted=安装未完成。%n%n请更正问题后重新运行安装程序。
AbortRetryIgnoreSelectAction=选择操作
AbortRetryIgnoreRetry=重试(&T)
AbortRetryIgnoreIgnore=忽略错误并继续(&I)
AbortRetryIgnoreCancel=取消安装
RetryCancelSelectAction=选择操作
RetryCancelRetry=重试(&T)
RetryCancelCancel=取消

; *** 安装状态消息
StatusClosingApplications=正在关闭应用程序...
StatusCreateDirs=正在创建目录...
StatusExtractFiles=正在解压文件...
StatusDownloadFiles=正在下载文件...
StatusCreateIcons=正在创建快捷方式...
StatusCreateIniEntries=正在创建 INI 项...
StatusCreateRegistryEntries=正在创建注册表项...
StatusRegisterFiles=正在注册文件...
StatusSavingUninstall=正在保存卸载信息...
StatusRunProgram=正在完成安装...
StatusRestartingApplications=正在重新启动应用程序...
StatusRollback=正在回滚更改...

; *** 其他错误
ErrorInternal2=内部错误: %1
ErrorFunctionFailedNoCode=%1 失败
ErrorFunctionFailed=%1 失败；代码 %2
ErrorFunctionFailedWithMessage=%1 失败；代码 %2。%n%3
ErrorExecutingProgram=无法执行文件:%n%1

; *** 注册表错误
ErrorRegOpenKey=打开注册表项时出错:%n%1\%2
ErrorRegCreateKey=创建注册表项时出错:%n%1\%2
ErrorRegWriteKey=写入注册表项时出错:%n%1\%2

; *** INI 错误
ErrorIniEntry=在文件“%1”中创建 INI 项时出错。

; *** 文件复制错误
FileAbortRetryIgnoreSkipNotRecommended=跳过此文件（不推荐）(&S)
FileAbortRetryIgnoreIgnoreNotRecommended=忽略错误并继续（不推荐）(&I)
SourceIsCorrupted=源文件已损坏
SourceDoesntExist=源文件“%1”不存在
SourceVerificationFailed=源文件校验失败: %1
VerificationSignatureDoesntExist=签名文件“%1”不存在
VerificationSignatureInvalid=签名文件“%1”无效
VerificationKeyNotFound=签名文件“%1”使用了未知密钥
VerificationFileNameIncorrect=文件名称不正确
VerificationFileTagIncorrect=文件标记不正确
VerificationFileSizeIncorrect=文件大小不正确
VerificationFileHashIncorrect=文件哈希不正确
ExistingFileReadOnly2=现有文件被标记为只读，无法替换。
ExistingFileReadOnlyRetry=移除只读属性并重试(&R)
ExistingFileReadOnlyKeepExisting=保留现有文件(&K)
ErrorReadingExistingDest=读取现有文件时发生错误:
FileExistsSelectAction=选择操作
FileExists2=文件已存在。
FileExistsOverwriteExisting=覆盖现有文件(&O)
FileExistsKeepExisting=保留现有文件(&K)
FileExistsOverwriteOrKeepAll=后续冲突执行相同操作(&D)
ExistingFileNewerSelectAction=选择操作
ExistingFileNewer2=现有文件比安装程序要安装的文件更新。
ExistingFileNewerOverwriteExisting=覆盖现有文件(&O)
ExistingFileNewerKeepExisting=保留现有文件（推荐）(&K)
ExistingFileNewerOverwriteOrKeepAll=后续冲突执行相同操作(&D)
ErrorChangingAttr=更改现有文件属性时发生错误:
ErrorCreatingTemp=在目标目录中创建文件时发生错误:
ErrorReadingSource=读取源文件时发生错误:
ErrorCopying=复制文件时发生错误:
ErrorDownloading=下载文件时发生错误:
ErrorExtracting=解压归档文件时发生错误:
ErrorReplacingExistingFile=替换现有文件时发生错误:
ErrorRestartReplace=RestartReplace 失败:
ErrorRenamingTemp=重命名目标目录中的文件时发生错误:
ErrorRegisterServer=无法注册 DLL/OCX: %1
ErrorRegSvr32Failed=RegSvr32 失败，退出代码 %1
ErrorRegisterTypeLib=无法注册类型库: %1

; *** 卸载显示名称标记
UninstallDisplayNameMark=%1 (%2)
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32 位
UninstallDisplayNameMark64Bit=64 位
UninstallDisplayNameMarkAllUsers=所有用户
UninstallDisplayNameMarkCurrentUser=当前用户

; *** 安装后错误
ErrorOpeningReadme=打开 README 文件时发生错误。
ErrorRestartingComputer=安装程序无法重启计算机。请手动重启。

; *** 卸载程序消息
UninstallNotFound=文件“%1”不存在。无法卸载。
UninstallOpenError=文件“%1”无法打开。无法卸载
UninstallUnsupportedVer=卸载日志文件“%1”的格式不被当前卸载程序识别。无法卸载
UninstallUnknownEntry=在卸载日志中遇到了未知条目 (%1)
ConfirmUninstall=确定要完全删除 %1 及其所有组件吗？
UninstallOnlyOnWin64=本安装只能在 64 位 Windows 上卸载。
OnlyAdminCanUninstall=本安装只能由具有管理员权限的用户卸载。
UninstallStatusLabel=请稍候，正在从计算机中移除 %1。
UninstalledAll=%1 已成功从计算机上移除。
UninstalledMost=%1 卸载完成。%n%n某些元素无法移除，可手动删除。
UninstalledAndNeedsRestart=为完成 %1 的卸载，必须重启计算机。%n%n现在重启吗？
UninstallDataCorrupted=“%1”文件已损坏。无法卸载

; *** 卸载阶段消息
ConfirmDeleteSharedFileTitle=删除共享文件？
ConfirmDeleteSharedFile2=系统显示以下共享文件不再被任何程序使用。要让卸载程序移除该共享文件吗？%n%n如果仍有程序在使用此文件而它被移除，这些程序可能无法正常运行。不确定请选择“否”。把文件留在系统中不会造成危害。
SharedFileNameLabel=文件名:
SharedFileLocationLabel=位置:
WizardUninstalling=卸载状态
StatusUninstalling=正在卸载 %1...

; *** 关机阻止原因
ShutdownBlockReasonInstallingApp=正在安装 %1。
ShutdownBlockReasonUninstallingApp=正在卸载 %1。

; *** 自定义消息（脚本中使用）
[CustomMessages]

NameAndVersion=%1 版本 %2
AdditionalIcons=附加快捷方式:
CreateDesktopIcon=创建桌面快捷方式(&D)
CreateQuickLaunchIcon=创建快速启动快捷方式(&Q)
ProgramOnTheWeb=%1 主页
UninstallProgram=卸载 %1
LaunchProgram=启动 %1
AssocFileExtension=将 %1 与 %2 文件扩展名关联(&A)
AssocingFileExtension=正在将 %1 与 %2 文件扩展名关联...
AutoStartProgramGroupDescription=启动:
AutoStartProgram=自动启动 %1
AddonHostProgramNotFound=在你选择的文件夹中找不到 %1。%n%n仍要继续吗？
