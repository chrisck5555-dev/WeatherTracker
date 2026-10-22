; The AppId must NEVER change: it is how a newer Setup upgrades the old install.
; Needs Inno Setup 6.6 or newer (for the dark-mode wizard images).
#define MyAppName "Weather Tracker"
#define MyAppVersion "1.0.0"
#define MyAppExe "WeatherTracker.exe"

[Setup]
AppId={{B7A1C3E2-5D4F-4A86-9C21-7E3F0A9D6B15}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Weather Tracker
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\installer-output
OutputBaseFilename=WeatherTracker-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

; Look of the Setup window
SetupIconFile=..\resources\app-icon\app.ico
UninstallDisplayIcon={app}\{#MyAppExe}
WizardStyle=modern dynamic
WizardImageFile=..\resources\installer\wizard-light-202x386.png,..\resources\installer\wizard-light-269x515.png,..\resources\installer\wizard-light-336x643.png,..\resources\installer\wizard-light-403x772.png,..\resources\installer\wizard-light-430x824.png,..\resources\installer\wizard-light-498x953.png,..\resources\installer\wizard-light-534x1022.png
WizardImageFileDynamicDark=..\resources\installer\wizard-dark-202x386.png,..\resources\installer\wizard-dark-269x515.png,..\resources\installer\wizard-dark-336x643.png,..\resources\installer\wizard-dark-403x772.png,..\resources\installer\wizard-dark-430x824.png,..\resources\installer\wizard-dark-498x953.png,..\resources\installer\wizard-dark-534x1022.png
WizardSmallImageFile=..\resources\installer\wizard-small-58.png,..\resources\installer\wizard-small-77.png,..\resources\installer\wizard-small-97.png,..\resources\installer\wizard-small-116.png,..\resources\installer\wizard-small-124.png,..\resources\installer\wizard-small-143.png,..\resources\installer\wizard-small-159.png

[Tasks]
Name: "desktopicon"; Description: "Put a shortcut on the Desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "Open Weather Tracker now"; Flags: nowait postinstall skipifsilent
