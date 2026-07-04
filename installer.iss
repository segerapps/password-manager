; Inno Setup script for Password Manager.
; Build the app first:  cmake --build build --config Release
; Then compile this:    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
;   (or open in the Inno Setup IDE and press Build/Compile)
; Output: build\installer\PasswordManager-Setup-0.2.0.exe

#define MyAppName "Password Manager"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "Aditya Ilham"
#define MyAppExeName "password_manager.exe"

[Setup]
; Stable AppId — keep this the SAME across versions so upgrades replace cleanly.
AppId={{EC3D5FDC-0945-49A2-8EE9-BC976C5DCD3B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\PasswordManager
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Per-user install (no UAC / admin). Installs to %LOCALAPPDATA%\Programs.
PrivilegesRequired=lowest
OutputDir=build\installer
OutputBaseFilename=PasswordManager-Setup-{#MyAppVersion}
SetupIconFile=resources\app_icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Bundle the deployed Release tree, minus dev/test artefacts.
Source: "build\Release\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs; \
    Excludes: "pm_tests.exe,pm_core.lib,*.lib,*.pdb,*.ilk,*.exp"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
