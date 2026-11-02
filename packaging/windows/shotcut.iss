; -- BUGS! Video Editor installer, adapted from shotcut.iss --
; Copyright (c) 2024-2025 Meltytech, LLC

; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.

[Setup]
AppVersion=YY.MM.DD
AppId={{7E3C1B52-4C3A-4B7E-9A55-2F6B1D0E8A11}
AppPublisher=BUGS! Video Editor
AppName=BUGS! Video Editor
WizardStyle=modern
DefaultDirName={autopf}\BUGS Video Editor
; Since no icons will be created in "{group}", we don't need the wizard
; to ask for a Start Menu folder name:
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\bugs-video-editor.exe
LicenseFile=Shotcut\COPYING.txt
Compression=lzma2
SolidCompression=no
OutputDir=userdocs:Inno Setup Examples Output
; Installs for the current user with no administrator prompt, unless asked otherwise.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline dialog
; "ArchitecturesAllowed=x64" specifies that Setup cannot run on
; anything but x64.
ArchitecturesAllowed=x64compatible
; "ArchitecturesInstallIn64BitMode=x64" requests that the install be
; done in "64-bit mode" on x64, meaning it should use the native
; 64-bit Program Files directory and the 64-bit view of the registry.
ArchitecturesInstallIn64BitMode=x64compatible
AppMutex="BUGS Video Editor Running Mutex"
OutputBaseFilename=bugs-video-editor-setup
UninstallDisplayName=BUGS! Video Editor
VersionInfoCopyright="Based on Shotcut, Copyright (c) 2012-2026 Meltytech, LLC"
WizardSmallImageFile="shotcut-logo-64.bmp"
WizardImageStretch=yes
ShowLanguageDialog=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
en.StartMenu=Create Start Menu Shortcut
en.DesktopIcon=Create Desktop Shortcut (Icon)
en.ClearSettings=Remove BUGS! Settings From Registry
en.InstallingBugs=Installing BUGS! Video Editor...
en.StartBugs=Start BUGS! Video Editor

[Tasks]
Name: startMenu; Description: "{cm:StartMenu}"
Name: desktopIcon; Description: "{cm:DesktopIcon}"
Name: removeSettings; Description: "{cm:ClearSettings}"; Flags: unchecked

[Files]
Source: "Shotcut\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs uninsremovereadonly

[Icons]
Name: "{autoprograms}\BUGS! Video Editor"; Filename: "{app}\bugs-video-editor.exe"; Tasks: startMenu
Name: "{autodesktop}\BUGS! Video Editor"; Filename: "{app}\bugs-video-editor.exe"; Tasks: desktopIcon

[Registry]
Root: HKCU; Subkey: "Software\BUGS\BUGS! Video Editor"; Tasks: removeSettings; AfterInstall: RemoveSettings()

[Run]
Filename: "{app}\bugs-video-editor.exe"; Description: "{cm:StartBugs}"; Flags: postinstall nowait skipifsilent

[Code]
procedure RemoveSettings();
begin
  RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\BUGS\BUGS! Video Editor');
end;

procedure InitializeWizard();
var
  CustomStatusLabel: TNewStaticText;
begin
  // Hide radio buttons on License page and pre-select "accept" to enable "next" button
  WizardForm.LicenseAcceptedRadio.Checked := True;
  WizardForm.LicenseAcceptedRadio.Visible := False;
  WizardForm.LicenseNotAcceptedRadio.Visible := False;
  WizardForm.LicenseLabel1.Visible := False;
  WizardForm.LicenseMemo.Top := WizardForm.LicenseLabel1.Top;
  WizardForm.LicenseMemo.Height :=
    WizardForm.LicenseNotAcceptedRadio.Top +
    WizardForm.LicenseNotAcceptedRadio.Height -
    WizardForm.LicenseMemo.Top - ScaleY(5);

  WizardForm.FilenameLabel.Visible := False;
  WizardForm.StatusLabel.Visible := False;

  WizardForm.ProgressGauge.Top := WizardForm.InstallingPage.Height - ScaleY(60);

  CustomStatusLabel := TNewStaticText.Create(WizardForm);
  CustomStatusLabel.Parent := WizardForm.InstallingPage;
  CustomStatusLabel.Caption := ExpandConstant('{cm:InstallingBugs}');
  CustomStatusLabel.Font.Size := CustomStatusLabel.Font.Size + 4;
  CustomStatusLabel.Font.Style := [fsBold];
  CustomStatusLabel.AutoSize := True;
  CustomStatusLabel.Top :=
    WizardForm.ProgressGauge.Top - CustomStatusLabel.Height - ScaleY(8);
  CustomStatusLabel.Left :=
    WizardForm.ProgressGauge.Left +
    ((WizardForm.ProgressGauge.Width - CustomStatusLabel.Width) div 2);
end;

