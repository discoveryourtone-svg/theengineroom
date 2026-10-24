; The Engine Room — Windows installer (Inno Setup 6)
; Built by CI:  iscc /DVersion=1.0.0 /DArtDir=<path to Release artefacts> installer\windows\EngineRoom.iss

#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef ArtDir
  #define ArtDir "..\..\build\EngineRoom_artefacts\Release"
#endif

[Setup]
AppId={{6C1E4A52-8B3D-4F6E-9C2A-1D7E5B9A0F31}
AppName=The Engine Room
AppVersion={#Version}
AppVerName=The Engine Room {#Version}
AppPublisher=Discover Your Tone
AppPublisherURL=https://discoveryourtone.com
AppSupportURL=https://discoveryourtone.com/support
DefaultDirName={autopf}\Discover Your Tone\The Engine Room
DefaultGroupName=Discover Your Tone
DisableProgramGroupPage=yes
DisableDirPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
LicenseFile=..\EULA.txt
OutputDir=..\..\dist
OutputBaseFilename=TheEngineRoom-{#Version}-Windows-Setup
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\The Engine Room.exe
UninstallDisplayName=The Engine Room
WizardStyle=modern
WizardImageFile=wizard-100.bmp,wizard-125.bmp,wizard-150.bmp,wizard-200.bmp,wizard-250.bmp
WizardSmallImageFile=small-100.bmp,small-125.bmp,small-150.bmp,small-200.bmp,small-250.bmp
WizardImageStretch=no
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
VersionInfoCompany=Discover Your Tone
VersionInfoProductName=The Engine Room
VersionInfoVersion={#Version}

[Messages]
WelcomeLabel1=The Engine Room.
WelcomeLabel2=Overdrive, distortion and fuzz you can see.%n%nThis installs the VST3 plugin for your DAW and The Engine Room app, so you can play straight through your audio interface.%n%nHave your licence key ready, or skip it for now. Everything works free for 14 days.
FinishedHeadingLabel=You're ready to play.
FinishedLabel=Open your DAW and rescan plugins if The Engine Room doesn't show up. Then click ACTIVATE at the bottom of the pedal and paste your key.%n%nFind your sound.

[Types]
Name: "full";   Description: "Everything (recommended)"
Name: "plugin"; Description: "VST3 plugin only"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin (Ableton Live, Cubase, Reaper, Studio One, FL Studio...)"; Types: full plugin custom
Name: "standalone"; Description: "The Engine Room app (no DAW needed)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Put The Engine Room app on the desktop"; Components: standalone; Flags: unchecked

[InstallDelete]
; Clear out any old copy first so stale files never linger inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\The Engine Room.vst3"; Components: vst3

[Files]
Source: "{#ArtDir}\VST3\The Engine Room.vst3\*"; DestDir: "{commoncf64}\VST3\The Engine Room.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtDir}\Standalone\The Engine Room.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\EULA.txt"; DestDir: "{app}"; DestName: "Licence Agreement.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\The Engine Room"; Filename: "{app}\The Engine Room.exe"; Components: standalone
Name: "{autodesktop}\The Engine Room"; Filename: "{app}\The Engine Room.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\The Engine Room.exe"; Description: "Open The Engine Room now"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\The Engine Room.vst3"
