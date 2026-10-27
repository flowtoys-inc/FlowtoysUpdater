; Inno Setup script for the Windows installer, built in CI from the CMake
; Release output (see .github/workflows/release.yml).
;
; The app is built with a statically linked CRT, so no DLLs and no VC
; redistributable are bundled (the old redist step was broken anyway: it
; copied vc_redist.x64.exe but ran vcredist_x64.exe).
;
; AppId must never change: the upgrade flow uninstalls the previous
; version by looking up this id, including installs made by the 1.x
; installers already in the field.

#define ApplicationName 'FlowtoysUpdater'
#ifndef ApplicationVersion
  #define ApplicationVersion GetStringFileInfo('build\FlowtoysUpdater_artefacts\Release\FlowtoysUpdater.exe', "ProductVersion")
#endif

[Setup]
AppName={#ApplicationName}
AppId={#ApplicationName}
AppVersion={#ApplicationVersion}
AppPublisher=Ben Kuper
AppPublisherURL=http://www.flowtoys.com
DefaultDirName={autopf}\{#ApplicationName}
DefaultGroupName={#ApplicationName}
UninstallDisplayIcon={app}\{#ApplicationName}.exe
UninstallDisplayName={#ApplicationName}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
; The self-updater downloads artifacts named FlowtoysUpdater-win-x64-<version>.exe
OutputBaseFilename={#ApplicationName}-win-x64-{#ApplicationVersion}
SetupIconFile=setup.ico

[Messages]
SetupWindowTitle={#ApplicationName} {#ApplicationVersion} Setup

[Files]
Source: "build\FlowtoysUpdater_artefacts\Release\{#ApplicationName}.exe"; DestDir: "{app}"

[Icons]
Name: "{group}\{#ApplicationName}"; Filename: "{app}\{#ApplicationName}.exe"

[Run]
Filename: "{app}\{#ApplicationName}.exe"; Description: "{cm:LaunchProgram,{#ApplicationName}.exe}"; Flags: nowait postinstall skipifsilent

[Code]
function GetUninstallString(): String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  sUnInstPath := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#emit SetupSetting("AppId")}_is1');
  sUnInstallString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString);
  Result := sUnInstallString;
end;


/////////////////////////////////////////////////////////////////////
function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;


/////////////////////////////////////////////////////////////////////
function UnInstallOldVersion(): Integer;
var
  sUnInstallString: String;
  iResultCode: Integer;
begin
// Return Values:
// 1 - uninstall string is empty
// 2 - error executing the UnInstallString
// 3 - successfully executed the UnInstallString

  // default return value
  Result := 0;

  // get the uninstall string of the old app
  sUnInstallString := GetUninstallString();
  if sUnInstallString <> '' then begin
    sUnInstallString := RemoveQuotes(sUnInstallString);
    if Exec(sUnInstallString, '/SILENT /NORESTART /SUPPRESSMSGBOXES','', SW_HIDE, ewWaitUntilTerminated, iResultCode) then
      Result := 3
    else
      Result := 2;
  end else
    Result := 1;
end;

/////////////////////////////////////////////////////////////////////
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep=ssInstall) then
  begin
    if (IsUpgrade()) then
    begin
      UnInstallOldVersion();
    end;
  end;
end;
