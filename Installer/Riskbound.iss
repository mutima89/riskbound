; Riskbound Windows installer (Inno Setup 6). Built by Scripts\BuildInstaller.bat, which first stages the
; release folder with Scripts\MakePortableZip.ps1 so the installer and the portable zip ship identical files.
; Defines passed by the build script: AppVersion (e.g. 0.5.0-alpha), NumericVersion (e.g. 0.5.0.0), SourceDir.
; Saves live in %LOCALAPPDATA%\Riskbound and are never touched by install or uninstall.

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef NumericVersion
  #define NumericVersion "0.0.0.0"
#endif
#ifndef SourceDir
  #error SourceDir must point at the staged release folder (Release\Portable\Riskbound-<version>-Win64-Portable)
#endif

[Setup]
AppId={{6F1D4B2A-8C3E-4F57-9A0B-52C7E1D3A9F4}
AppName=Riskbound
AppVersion={#AppVersion}
AppVerName=Riskbound {#AppVersion}
AppPublisher=M A Almahlaki
AppPublisherURL=https://www.linkedin.com/in/mohammed-almahlaki001/
AppSupportURL=https://www.linkedin.com/in/mohammed-almahlaki001/
AppContact=m.awwadh.k@gmail.com
AppCopyright=Copyright (c) 2026 M A Almahlaki
VersionInfoVersion={#NumericVersion}
VersionInfoDescription=Riskbound installer
DefaultDirName={autopf}\Riskbound
DefaultGroupName=Riskbound
DisableProgramGroupPage=yes
; Installs per user without administrator rights by default; an administrator can choose all users.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
LicenseFile=..\Docs\Release\Licence-and-credits.txt
InfoBeforeFile=..\Docs\Release\UNSIGNED-BUILD.txt
OutputDir=..\Release\Installer
OutputBaseFilename=Riskbound-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Riskbound.exe
UninstallDisplayName=Riskbound {#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Riskbound"; Filename: "{app}\Riskbound.exe"; WorkingDir: "{app}"
Name: "{group}\Read me"; Filename: "{app}\README.txt"
Name: "{group}\Licences and credits"; Filename: "{app}\Licences"
Name: "{group}\{cm:UninstallProgram,Riskbound}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Riskbound"; Filename: "{app}\Riskbound.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Riskbound.exe"; Description: "{cm:LaunchProgram,Riskbound}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Anything the game wrote inside its own folder (it normally writes only to %LOCALAPPDATA%).
Type: filesandordirs; Name: "{app}\Riskbound\Saved"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and (not UninstallSilent) then
    MsgBox('Riskbound has been removed. Your saves were kept in ' + ExpandConstant('{localappdata}') +
      '\Riskbound. Delete that folder too if you want to remove them.', mbInformation, MB_OK);
end;
