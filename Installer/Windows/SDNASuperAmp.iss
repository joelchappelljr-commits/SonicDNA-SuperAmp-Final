#define MyAppName "SonicDNA SuperAmp"
#define MyAppVersion "0.7.1"
#define MyAppPublisher "SonicDNA"

[Setup]
AppId={{7F5160A7-D6D8-48D4-AD6D-5DE3F6421D70}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\SonicDNA\SuperAmp
DefaultGroupName=SonicDNA
OutputDir=..\..\installer-output
OutputBaseFilename=SonicDNA-SuperAmp-Windows-x64-ASIO-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
UninstallDisplayName={#MyAppName}
VersionInfoVersion=0.7.1.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} ASIO Installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Dirs]
Name: "{commoncf64}\VST3"

[InstallDelete]
Type: files; Name: "{app}\SonicDNA SuperAmp.exe"
Type: files; Name: "{group}\SonicDNA SuperAmp.lnk"
Type: files; Name: "{autodesktop}\SonicDNA SuperAmp.lnk"
Type: filesandordirs; Name: "{commoncf64}\VST3\SonicDNA SuperAmp.vst3"

[Files]
Source: "..\..\distribution\Standalone\SonicDNA SuperAmp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\distribution\VST3\SonicDNA SuperAmp.vst3\*"; DestDir: "{commoncf64}\VST3\SonicDNA SuperAmp.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs restartreplace

[Icons]
Name: "{group}\SonicDNA SuperAmp"; Filename: "{app}\SonicDNA SuperAmp.exe"
Name: "{autodesktop}\SonicDNA SuperAmp"; Filename: "{app}\SonicDNA SuperAmp.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  Vst3Path: String;
begin
  if CurStep = ssInstall then
  begin
    Vst3Path := ExpandConstant('{commoncf64}\VST3\SonicDNA SuperAmp.vst3');

    if FileExists(Vst3Path) then
    begin
      if not DeleteFile(Vst3Path) then
      begin
        MsgBox('An older SonicDNA SuperAmp VST3 file could not be removed.'#13#10 +
               'Close all DAWs and plugin hosts, then run the installer again.', mbError, MB_OK);
        Abort;
      end;
    end;

    if DirExists(Vst3Path) then
    begin
      if not DelTree(Vst3Path, True, True, True) then
      begin
        MsgBox('The previous SonicDNA SuperAmp VST3 folder could not be removed.'#13#10 +
               'Close all DAWs and plugin hosts, then run the installer again.', mbError, MB_OK);
        Abort;
      end;
    end;
  end;
end;
