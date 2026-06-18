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

#define MyAppName "ETestStudio 自动化测试系统"
#define MyCompanyName ""
#define MyAppPublisher ""
#define MyAppURL ""
#define MyAppMutex "ETestStudioAppMutex"

[Setup]
AppId={{6811a736-4ffd-4dae-a7a7-459a6e3d572f}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf64}\{#MyCompanyName}\{#MyProductName}
DefaultGroupName={#MyProductName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#WhereToOutput}
OutputBaseFilename={#MyProductName}-setup-x64-{#MyAppVersion}
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
Source: "{#WhereAreFiles}\vc_redist.x64.exe"; DestDir: "{app}"; Flags: ignoreversion

; Qt 运行时插件目录
Source: "{#WhereAreFiles}\bearer\*.dll"; DestDir: "{app}\bearer"; Flags: ignoreversion
Source: "{#WhereAreFiles}\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion
Source: "{#WhereAreFiles}\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#WhereAreFiles}\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "{#WhereAreFiles}\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "{#WhereAreFiles}\translations\*.qm"; DestDir: "{app}\translations"; Flags: ignoreversion

; 内置模拟设备插件
Source: "{#WhereAreFiles}\plugins\*.dll"; DestDir: "{app}\plugins"; Flags: ignoreversion

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
