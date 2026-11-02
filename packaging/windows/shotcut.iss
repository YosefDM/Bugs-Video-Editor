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
; No "for me or for everyone?" question; only the command line can change it.
PrivilegesRequiredOverridesAllowed=commandline
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
; BUGS! artwork, at 100%, 150% and 200% display scaling.
SetupIconFile=bugs-setup.ico
WizardImageFile=bugs-wizard-100.bmp,bugs-wizard-150.bmp,bugs-wizard-200.bmp
WizardSmallImageFile=bugs-wizard-small-100.bmp,bugs-wizard-small-150.bmp,bugs-wizard-small-200.bmp
WizardImageStretch=no
ShowLanguageDialog=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
en.StartMenu=Add BUGS! to the Start menu
en.DesktopIcon=Put a BUGS! shortcut on the desktop
en.ClearSettings=Start fresh (forget the library list and light or dark choice)
en.InstallingBugs=Installing BUGS! Video Editor...
en.StartBugs=Open BUGS! Video Editor now

[Tasks]
Name: startMenu; Description: "{cm:StartMenu}"
Name: desktopIcon; Description: "{cm:DesktopIcon}"
Name: removeSettings; Description: "{cm:ClearSettings}"; Flags: unchecked

[Files]
Source: "Shotcut\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs uninsremovereadonly
Source: "bugs-spider-*.bmp"; Flags: dontcopy

[Icons]
Name: "{autoprograms}\BUGS! Video Editor"; Filename: "{app}\bugs-video-editor.exe"; Tasks: startMenu
Name: "{autodesktop}\BUGS! Video Editor"; Filename: "{app}\bugs-video-editor.exe"; Tasks: desktopIcon

[Registry]
Root: HKCU; Subkey: "Software\BUGS\BUGS! Video Editor"; Tasks: removeSettings; AfterInstall: RemoveSettings()

[Run]
Filename: "{app}\bugs-video-editor.exe"; Description: "{cm:StartBugs}"; Flags: postinstall nowait skipifsilent

[Code]
// A prank for Levi: when the install starts, a huntsman spider scuttles across the installer.
// It runs in its own see-through window (magenta is invisible), so only the spider shows.
// Photo: Heteropoda venatoria by ja:user:Dieno, public domain, via Wikimedia Commons.
// Skipped for silent installs.
const
  GWL_EXSTYLE = -20;
  WS_EX_TRANSPARENT = $20;
  WS_EX_TOOLWINDOW = $80;
  WS_EX_LAYERED = $80000;
  LWA_COLORKEY = 1;
  SeeThrough = $FF00FF;

function GetWindowLong(Wnd: HWND; Index: Integer): Longint;
  external 'GetWindowLongW@user32.dll stdcall';
function SetWindowLong(Wnd: HWND; Index: Integer; NewLong: Longint): Longint;
  external 'SetWindowLongW@user32.dll stdcall';
function SetLayeredWindowAttributes(Wnd: HWND; Key: Cardinal; Alpha: Byte; Flags: Cardinal): Integer;
  external 'SetLayeredWindowAttributes@user32.dll stdcall';

procedure CrawlSpider();
var
  Form: TSetupForm;
  Frames: array[0..2] of TBitmapImage;
  Size, StartX, StartY, DistX, DistY, Steps, I, Shown, Next: Integer;
  Name: String;
begin
  // Pictures are made at exact sizes for 100%, 150% and 200% display scaling. Stretching them
  // would blend the see-through magenta into the spider's edges.
  if ScaleY(100) >= 190 then
    Size := 920
  else if ScaleY(100) >= 140 then
    Size := 690
  else
    Size := 460;
  for I := 1 to 3 do
    ExtractTemporaryFile('bugs-spider-' + IntToStr(Size) + '-' + IntToStr(I) + '.bmp');
  WizardForm.Repaint;

  Form := CreateCustomForm;
  try
    Form.BorderStyle := bsNone;
    Form.FormStyle := fsStayOnTop;
    Form.Color := SeeThrough;
    Form.ClientWidth := Size;
    Form.ClientHeight := Size;
    for I := 0 to 2 do
    begin
      Frames[I] := TBitmapImage.Create(Form);
      Frames[I].Parent := Form;
      Frames[I].SetBounds(0, 0, Size, Size);
      Frames[I].Stretch := False;
      Name := 'bugs-spider-' + IntToStr(Size) + '-' + IntToStr(I + 1) + '.bmp';
      Frames[I].Bitmap.LoadFromFile(ExpandConstant('{tmp}\') + Name);
      Frames[I].Visible := I = 1;
    end;
    SetWindowLong(Form.Handle, GWL_EXSTYLE,
      GetWindowLong(Form.Handle, GWL_EXSTYLE) or WS_EX_LAYERED or WS_EX_TRANSPARENT or WS_EX_TOOLWINDOW);
    SetLayeredWindowAttributes(Form.Handle, SeeThrough, 255, LWA_COLORKEY);

    // From off the lower left, up across the window and off the top right. The photo is turned
    // to face this way.
    DistX := WizardForm.Width + Size;
    DistY := -(DistX * 445) div 1000;
    StartX := WizardForm.Left - Size;
    StartY := WizardForm.Top + WizardForm.Height - Size div 4;
    Form.Left := StartX;
    Form.Top := StartY;
    Form.Show;

    Steps := 90;
    Shown := 1;
    for I := 1 to Steps do
    begin
      Form.Left := StartX + DistX * I div Steps;
      Form.Top := StartY + DistY * I div Steps;
      // Legs: flick between the three poses.
      case I mod 4 of
        0, 2: Next := 1;
        1: Next := 0;
      else
        Next := 2;
      end;
      Frames[Next].Visible := True;
      if Next <> Shown then
        Frames[Shown].Visible := False;
      Shown := Next;
      Form.Repaint;
      Sleep(11);
      // Real spiders move in bursts. It freezes in the middle of the window for a moment.
      if I = Steps div 2 then
        Sleep(1300)
      else if I mod 15 = 0 then
        Sleep(180 + Random(320));
    end;
  finally
    Form.Free;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpInstalling) and not WizardSilent then
    CrawlSpider();
end;

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

