; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
AppName=NetXMS Agent
AppVerName=NetXMS Agent 0.2.0
AppVersion=0.2.0
AppPublisher=NetXMS Team
AppPublisherURL=http://www.netxms.org
AppSupportURL=http://www.netxms.org
AppUpdatesURL=http://www.netxms.org
DefaultDirName=C:\NetXMS
DefaultGroupName=NetXMS Agent
AllowNoIcons=yes
OutputBaseFilename=nxagent-0.2.0
Compression=lzma
SolidCompression=yes
LanguageDetectionMethod=none
LicenseFile=..\..\..\copying

[Files]
Source: "..\..\libnetxms\Release\libnetxms.dll"; DestDir: "{app}\bin"; BeforeInstall: StopService; Flags: ignoreversion
Source: "..\..\libnxcscp\Release\libnxcscp.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\core\Release\nxagentd.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\subagents\winperf\Release\winperf.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\subagents\ping\Release\ping.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\subagents\portCheck\Release\portcheck.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\..\..\contrib\nxagentd.conf-dist"; DestDir: "{app}\etc"; Flags: ignoreversion

[Dirs]
Name: "{app}\etc"
Name: "{app}\var"

[Run]
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-c {app}\etc\nxagentd.conf -I"; WorkingDir: "{app}\bin"; StatusMsg: "Installing service..."; Flags: runhidden
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-s"; WorkingDir: "{app}\bin"; StatusMsg: "Starting service..."; Flags: runhidden

[UninstallRun]
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-S"; StatusMsg: "Stopping service..."; RunOnceId: "StopService"; Flags: runhidden
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-R"; StatusMsg: "Uninstalling service..."; RunOnceId: "DelService"; Flags: runhidden

[Code]
Procedure StopService;
Var
  strExecName : String;
  iResult : Integer;
Begin
  strExecName := ExpandConstant('{app}\bin\nxagentd.exe');
  If FileExists(strExecName) Then
  Begin
    Exec(strExecName, '-S', ExpandConstant('{app}\bin'), 0, ewWaitUntilTerminated, iResult);
  End;
End;

