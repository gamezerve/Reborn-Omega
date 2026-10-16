#ifndef RebornBuildRank
  #define RebornBuildRank "0"
#endif

#ifndef RebornDisplayName
  #define RebornDisplayName "Reborn Omega"
#endif

#ifndef RebornVersion
  #define RebornVersion "Unknown"
#endif

#ifndef RebornInstallerName
  #define RebornInstallerName "ZeroHourRebornOmega_Setup"
#endif

#ifndef RebornOutputDir
  #define RebornOutputDir "D:\TEMP"
#endif

#ifndef RebornSourceDir
  #define RebornSourceDir "D:\TEMP"
#endif

[Setup]
AppId={{REBORN-OMEGA}}
AppName=Zero Hour Reborn Omega
AppVersion={#RebornVersion}
AppVerName=Zero Hour {#RebornDisplayName}
DefaultDirName={code:GetInstallDir}
; Reborn: Use the folder selected in Browse exactly, without appending the default directory name.
AppendDefaultDirName=no
DisableDirPage=no
DisableProgramGroupPage=yes
AlwaysShowDirOnReadyPage=yes
OutputDir={#RebornOutputDir}
OutputBaseFilename={#RebornInstallerName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DirExistsWarning=no
UninstallDisplayName=Zero Hour {#RebornDisplayName}
UninstallDisplayIcon={app}\{#RebornDisplayName}.exe
Uninstallable=yes
CreateUninstallRegKey=yes
AppPublisher=Gamezerve

[Files]
; Reborn: Diagnostic symbols remain in the preparation folder and must never be shipped in the installer.
Source: "{#RebornSourceDir}\*"; DestDir: "{app}"; Excludes: "*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKCU; Subkey: "Software\RebornOmega"; ValueType: string; ValueName: "DisplayName"; ValueData: "{#RebornDisplayName}"
Root: HKCU; Subkey: "Software\RebornOmega"; ValueType: dword; ValueName: "BuildRank"; ValueData: "{#RebornBuildRank}"
Root: HKCU; Subkey: "Software\RebornOmega"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Icons]
Name: "{commondesktop}\{#RebornDisplayName}"; Filename: "{app}\{#RebornDisplayName}.exe"; Tasks: desktopicon

[Code]

function NormalizeZeroHourDir(Path: String): String;
begin
  Result := Path;

  if Pos('\ORIGIN\COMMAND AND CONQUER GENERALS ZERO HOUR', UpperCase(Path)) > 0 then
    Result := AddBackslash(Path) + 'Command and Conquer Generals Zero Hour';
end;

function GetInstallDir(Param: String): String;
begin
  if RegQueryStringValue(HKLM,
    'SOFTWARE\WOW6432Node\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour',
    'InstallPath',
    Result) then
  begin
    Result := NormalizeZeroHourDir(Result);
    Exit;
  end;

  if RegQueryStringValue(HKLM,
    'SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour',
    'InstallPath',
    Result) then
  begin
    Result := NormalizeZeroHourDir(Result);
    Exit;
  end;

  Result := '';
end;

procedure InitializeWizard;
begin
  if WizardDirValue = '' then
  begin
    MsgBox(
      'Command & Conquer Generals Zero Hour installation could not be detected automatically.' + #13#10 + #13#10 +
      'Please select your Zero Hour installation folder manually.',
      mbInformation,
      MB_OK);
  end;
end;

function HasOldRebornExecutables: Boolean;
var
  FindRec: TFindRec;
begin
  Result := False;

  if FindFirst(WizardDirValue + '\Reborn Omega*.exe', FindRec) then
  begin
    Result := True;
    FindClose(FindRec);
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  DataDir: String;
begin
  Result := True;

  if CurPageID = wpSelectDir then
  begin
    if WizardDirValue = '' then
    begin
      MsgBox(
        'Please select your Command & Conquer Generals Zero Hour installation folder before continuing.',
        mbError,
        MB_OK);

      Result := False;
      Exit;
    end;

    DataDir := WizardDirValue + '\RebornOmegaData';

    if DirExists(DataDir) or HasOldRebornExecutables then
    begin
      if MsgBox(
        'The existing Reborn Omega installation will be replaced.' + #13#10 + #13#10 +
        'The following will be removed before installation:' + #13#10 +
        '- RebornOmegaData' + #13#10 +
        '- Previous Reborn Omega executable files' + #13#10 + #13#10 +
        'Installation folder:' + #13#10 +
        WizardDirValue + #13#10 + #13#10 +
        'Do you want to continue?',
        mbConfirmation,
        MB_YESNO) <> IDYES then
      begin
        Result := False;
        Exit;
      end;
    end;
  end;
end;

function InitializeSetup(): Boolean;
var
  InstalledName: String;
  InstalledRank: Cardinal;
begin
  Result := True;

  if RegQueryStringValue(HKCU, 'Software\RebornOmega', 'DisplayName', InstalledName) and
     RegQueryDWordValue(HKCU, 'Software\RebornOmega', 'BuildRank', InstalledRank) then
  begin
    if InstalledRank = {#RebornBuildRank} then
      Result := MsgBox(InstalledName + ' is already installed.' + #13#10 + #13#10 + 'Do you want to reinstall it?', mbConfirmation, MB_YESNO) = IDYES
    else if InstalledRank < {#RebornBuildRank} then
      Result := MsgBox('An older version is already installed:' + #13#10 + InstalledName + #13#10 + #13#10 + 'Setup will update it to {#RebornDisplayName}.' + #13#10 + #13#10 + 'Continue?', mbConfirmation, MB_YESNO) = IDYES
    else
      Result := MsgBox('A newer version is already installed:' + #13#10 + InstalledName + #13#10 + #13#10 + 'You are about to install {#RebornDisplayName}.' + #13#10 + 'This may downgrade your installation.' + #13#10 + #13#10 + 'Continue?', mbConfirmation, MB_YESNO) = IDYES;
  end;
end;

procedure DeleteOldRebornExecutables;
var
  FindRec: TFindRec;
begin
  if FindFirst(ExpandConstant('{app}\Reborn Omega*.exe'), FindRec) then
  begin
    try
      repeat
        DeleteFile(ExpandConstant('{app}\') + FindRec.Name);
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  DataDir: String;
  ResultCode: Integer;
begin
  if CurStep = ssInstall then
  begin
    DataDir := ExpandConstant('{app}\RebornOmegaData');

    if DirExists(DataDir) then
      DelTree(DataDir, True, True, True);

    DeleteOldRebornExecutables;
  end
  else if CurStep = ssDone then
  begin
    if MsgBox(
      'It is recommended to restart your computer before using Reborn Omega.' + #13#10#13#10 +
      'Would you like to restart your computer now?',
      mbConfirmation,
      MB_YESNO) = IDYES then
    begin
      Exec(
        ExpandConstant('{sys}\shutdown.exe'),
        '/r /t 0',
        '',
        SW_HIDE,
        ewNoWait,
        ResultCode);
    end;
  end;
end;

[Messages]
ReadyLabel1=Setup is now ready to begin installing {#RebornDisplayName} on your computer.
ReadyLabel2a=Click Install to continue with the installation, or click Back if you want to review or change any settings.
SelectDirDesc=Where should {#RebornDisplayName} be installed?
SelectDirLabel3=Setup will install {#RebornDisplayName} into the following folder.
FinishedHeadingLabel=Installation Complete
FinishedLabel=Zero Hour {#RebornDisplayName} has been installed successfully.

[Run]
Filename: "{app}\RebornOmegaData\Tools\VC_redist.x86.exe"; \
    Description: "Install/Update Microsoft Visual C++ Runtime (Recommended)"; \
    Parameters: "/install /passive /norestart"; \
    Flags: postinstall skipifsilent waituntilterminated
