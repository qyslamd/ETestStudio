; Script created by zhouyonghua 2026-06-18.
; Inno Setup 6 脚本 — 打包 ETestStudio 安装包

; 所有宏由 CMake ISCC 调用时通过 /D 传入
#ifndef WhereAreFiles
#define WhereAreFiles "..\build\ninja-release\bin"
#endif

#ifndef WhereToOutput
#define WhereToOutput "..\..\dist"
#endif

#ifndef MyAppExeName
#define MyAppExeName "ETestStudio.exe"
#endif

#ifndef MyAppVersion
#define MyAppVersion "1.0.0"
#endif

#ifndef MySetupIcon
#define MySetupIcon "resources\icons\app_icon.ico"
#endif

#ifndef MyProductName
#define MyProductName "ETestStudio"
#endif

; 目标架构：x64（默认）或 x86。由 CMake 通过 /DMyAppArch 传入。
; x64 与 x86 使用不同 AppId，两者可在同一台机器共存，互不干扰。
#ifndef MyAppArch
  #define MyAppArch "x64"
#endif

#if MyAppArch == "x64"
  ; x64：64 位安装，进 64 位 Program Files，使用 x64 vc_redist
  #define MyAppId "{{6811a736-4ffd-4dae-a7a7-459a6e3d572f}"
  #define MyAppPf "{autopf64}"
  #define MyVcRedist "vc_redist.x64.exe"
  #define MyAppArchSuffix "x64"
#elif MyAppArch == "x86"
  ; x86：32 位安装，进 32 位 Program Files，使用 x86 vc_redist
  #define MyAppId "{{46AB6095-36F9-49F4-ABB1-6A36D8BB7E39}"
  #define MyAppPf "{autopf}"
  #define MyVcRedist "vc_redist.x86.exe"
  #define MyAppArchSuffix "x86"
#else
  #error MyAppArch 必须是 x64 或 x86
#endif

#define MyAppName "ETestStudio 自动化测试系统"
#define MyCompanyName ""
#define MyAppPublisher ""
#define MyAppURL ""
#define MyAppMutex "ETestStudioAppMutex"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={#MyAppPf}\{#MyCompanyName}\{#MyProductName}
DefaultGroupName={#MyProductName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
#if MyAppArch == "x64"
ArchitecturesInstallIn64BitMode=x64compatible
#endif
OutputDir={#WhereToOutput}
OutputBaseFilename={#MyProductName}-setup-{#MyAppArchSuffix}-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#MySetupIcon}
AppMutex={#MyAppMutex}
Uninstallable=yes
UninstallDisplayName={#MyAppName}

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkablealone

[Files]
; ETestStudio 主程序与 Qt 运行时
Source: "{#WhereAreFiles}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#WhereAreFiles}\Qt5*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#WhereAreFiles}\{#MyVcRedist}"; DestDir: "{app}"; Flags: ignoreversion

; Qt 运行时插件目录
Source: "{#WhereAreFiles}\bearer\*.dll"; DestDir: "{app}\bearer"; Flags: ignoreversion
Source: "{#WhereAreFiles}\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion
Source: "{#WhereAreFiles}\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#WhereAreFiles}\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "{#WhereAreFiles}\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "{#WhereAreFiles}\translations\*.qm"; DestDir: "{app}\translations"; Flags: ignoreversion

#ifexist "{#WhereAreFiles}\plugins"
; 内置模拟设备插件
Source: "{#WhereAreFiles}\plugins\*.dll"; DestDir: "{app}\plugins"; Flags: ignoreversion
#endif

#ifexist "{#WhereAreFiles}\test_projects"
; 测试工程（调试用）
Source: "{#WhereAreFiles}\test_projects\*"; DestDir: "{app}\test_projects"; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
const
  CSIDL_LOCAL_APPDATA = $001c;

function InitializeSetup(): Boolean;
var
  ErrorCode: Integer;
begin
  if CheckForMutexes('{#MyAppMutex}') then
  begin
    if MsgBox('检测到 ETestStudio 正在运行，是否关闭后继续安装？',
              mbConfirmation, MB_YESNO) = IDYES then
    begin
      ShellExec('open', ExpandConstant('{cmd}'),
        '/c taskkill /f /t /im ETestStudio.exe', '',
        SW_HIDE, ewNoWait, ErrorCode);
      Result := True;
    end
    else
      Result := False;
  end
  else
    Result := True;
end;

procedure DelConfigAndCache();
var
  LocalAppData: String;
begin
  LocalAppData := GetShellFolderByCSIDL(CSIDL_LOCAL_APPDATA, True);
  if LocalAppData <> '' then
    DelTree(LocalAppData + '\ETestStudio', True, True, True);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    DelConfigAndCache();
end;
