[Setup]
AppName=WDD
AppVersion=1.0
DefaultDirName={commonpf}\wdd
DefaultGroupName=Windows DD
OutputDir=setups
OutputBaseFilename=wdd-setup-1.0
UninstallDisplayIcon={app}\wdd.exe
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
ChangesEnvironment=yes

[Files]
Source:"wdd.exe"; \
  DestDir:"{app}"; Flags: ignoreversion

[Code]
procedure AddToPath();
var
  Path: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Path)
  then
    Path := '';

  if Pos(LowerCase(ExpandConstant('{app}')), LowerCase(Path)) = 0 then begin
    if (Path <> '') and (Path[Length(Path)] <> ';') then
      Path := Path + ';';
    Path := Path + ExpandConstant('{app}');
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,
      'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Path);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    AddToPath();
end;
