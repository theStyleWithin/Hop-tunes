; Inno Setup script for the Hop-tunes installer.
; Build:  ISCC /DAppVersion=1.4.0 /DStageDir=<staged folder> /DOutDir=<output folder> hop-tunes.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "stage"
#endif
#ifndef OutDir
  #define OutDir "dist"
#endif

[Setup]
AppId={{0E531854-F66F-46E0-9E27-5304DCAC3E97}
AppName=Hop-tunes
AppVersion={#AppVersion}
AppVerName=Hop-tunes {#AppVersion}
AppPublisher=theStyleWithin
AppPublisherURL=https://github.com/theStyleWithin/Hop-tunes
AppSupportURL=https://github.com/theStyleWithin/Hop-tunes/issues
DefaultDirName={autopf}\Hop-tunes
DefaultGroupName=Hop-tunes
DisableProgramGroupPage=yes
; Per-user by default (no administrator prompt); the wizard offers "for all users" too.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
InfoBeforeFile=windows-notice.txt
LicenseFile=..\..\LICENSE
SetupIconFile=hop-tunes.ico
UninstallDisplayIcon={app}\hop-tunes.exe
OutputDir={#OutDir}
OutputBaseFilename=Hop-tunes-Setup-{#AppVersion}-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Hop-tunes"; Filename: "{app}\hop-tunes.exe"
Name: "{autodesktop}\Hop-tunes"; Filename: "{app}\hop-tunes.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\hop-tunes.exe"; Description: "Start Hop-tunes"; Flags: nowait postinstall skipifsilent

; Your profiles, playlists and settings live in your user folder (%LOCALAPPDATA%\hop-tunes), not here,
; so uninstalling never deletes them.
