; Quake RT installer (Inno Setup 6).
; Build: tools/scripts package the RT build into Dist\QuakeRT first, then
;   ISCC.exe /DAppVersion=2.0.0 /DSourceDir=<Dist\QuakeRT> /DRedist=<vc_redist.x64.exe> Packaging\Windows\QuakeRT.iss
; scripts\make-installer.ps1 does all of that.
;
; Per-user install (no admin needed) into %LOCALAPPDATA%\Programs\QuakeRT. The only elevated step is the
; Microsoft Visual C++ runtime, installed only when it is missing or older than the one we build with.

#ifndef AppVersion
  #define AppVersion "2.0.1"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\Dist\QuakeRT"
#endif
#ifndef Redist
  #error Pass /DRedist=<path to vc_redist.x64.exe>
#endif

[Setup]
AppId={{6D3C2B6E-4E1A-4C47-9B1E-5A2F0C3D7E11}
AppName=Quake RT
AppVersion={#AppVersion}
AppVerName=Quake RT {#AppVersion}
AppPublisher=Quake RT community build
AppPublisherURL=https://github.com/Bliggs1984/vkquake-rt
AppSupportURL=https://github.com/Bliggs1984/vkquake-rt/issues
DefaultDirName={autopf}\QuakeRT
DefaultGroupName=Quake RT
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\..\Dist
OutputBaseFilename=QuakeRT-Setup-{#AppVersion}
SetupIconFile=..\..\Windows\vkQuake.ico
UninstallDisplayIcon={app}\vkQuake.exe
UninstallDisplayName=Quake RT
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\..\LICENSE.txt
InfoBeforeFile=installer-requirements.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Put a Quake RT icon on the desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "HOW-TO-PLAY.txt"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "HOW-TO-PLAY.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "{#Redist}"; DestDir: "{tmp}"; DestName: "vc_redist.x64.exe"; Flags: deleteafterinstall; Check: VCRedistNeeded

[Icons]
Name: "{group}\Quake RT"; Filename: "{app}\vkQuake.exe"; Parameters: "-prefremaster"; WorkingDir: "{app}"
Name: "{group}\How to play"; Filename: "{app}\HOW-TO-PLAY.txt"
Name: "{group}\Steam launch option"; Filename: "{app}\Steam launch option.txt"
Name: "{group}\Uninstall Quake RT"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Quake RT"; Filename: "{app}\vkQuake.exe"; Parameters: "-prefremaster"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /passive /norestart"; StatusMsg: "Installing the Microsoft Visual C++ runtime..."; Flags: shellexec waituntilterminated; Check: VCRedistNeeded
Filename: "{app}\vkQuake.exe"; Parameters: "-prefremaster"; WorkingDir: "{app}"; Description: "Play Quake RT now"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: files; Name: "{app}\Steam launch option.txt"

[Code]
// The engine and RayTracedGL1 are built with MSVC 14.44: the installed runtime must be at least that new.
function VCRedistNeeded: Boolean;
var
  Installed, Major, Minor: Cardinal;
begin
  Result := True;
  if RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Major', Major) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Minor', Minor) then
    Result := not ((Installed = 1) and ((Major > 14) or ((Major = 14) and (Minor >= 44))));
end;

function SteamQuakeInstalled: Boolean;
var
  Installed: Cardinal;
begin
  Result := RegQueryDWordValue(HKCU, 'Software\Valve\Steam\Apps\2310', 'Installed', Installed) and (Installed = 1);
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  if not SteamQuakeInstalled then
    Result := MsgBox('Quake from Steam doesn''t seem to be installed on this PC.' + #13#10 + #13#10 +
      'Quake RT needs your own copy of Quake (Steam, GOG or Epic Games Store).' + #13#10 +
      'If you have it from GOG or Epic, click Yes to carry on.' + #13#10 + #13#10 +
      'Carry on installing?', mbConfirmation, MB_YESNO) = IDYES;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    SaveStringToFile(ExpandConstant('{app}\Steam launch option.txt'),
      '"' + ExpandConstant('{app}\vkQuake.exe') + '" -prefremaster %command%' + #13#10, False);
end;
