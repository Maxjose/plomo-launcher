; Instalador del launcher de PLOMO (Inno Setup 6).
; Empaqueta la carpeta limpia que genera plomo\tools\package_stage.py (solo programa: sin cuentas, ajustes ni datos del juego).
; Lo compila .github\workflows\plomo-windows.yml: ISCC /DStageDir=<carpeta> plomo.iss

#define AppName "PLOMO"
#ifndef StageDir
  #define StageDir "..\..\stage"
#endif
; La version sale del propio PLOMO.exe (1.0.0.0); RemoveFileExt quita el ultimo ".0" y deja 1.0.0.
#ifndef AppVersion
  #define AppVersion RemoveFileExt(GetVersionNumbersString(StageDir + "\PLOMO.exe"))
#endif

[Setup]
; Identificador fijo: no cambiarlo nunca, permite que una version nueva se instale encima de la anterior.
AppId={{6E0F2A74-3C1B-4D59-9B7A-5D2C8F41A6E3}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=PLOMO
AppPublisherURL=https://playplomo.com/
AppSupportURL=https://playplomo.com/launcher/
; Instalacion por usuario: no pide permisos de administrador y el launcher puede escribir junto a su .exe.
DefaultDirName={localappdata}\Programs\PLOMO
DefaultGroupName=PLOMO
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
; Politica de privacidad: se muestra antes de instalar.
InfoBeforeFile=..\..\PRIVACY.md
DisableWelcomePage=no
WizardImageFile=assets\side-164x314.bmp,assets\side-246x471.bmp,assets\side-328x628.bmp
WizardSmallImageFile=assets\small-55x55.bmp,assets\small-83x83.bmp,assets\small-110x110.bmp
SetupIconFile=..\..\program_info\PLOMO.ico
UninstallDisplayIcon={app}\PLOMO.exe
UninstallDisplayName={#AppName}
OutputDir=output
OutputBaseFilename=PLOMO-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
; Si PLOMO esta abierto al actualizar, Windows ofrece cerrarlo en vez de fallar.
CloseApplications=yes
; PLOMO is started again by the [Run] entry below (only after an in-launcher update); Restart Manager must not do it too.
RestartApplications=no

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Messages]
spanish.WelcomeLabel2=Este asistente instalará el launcher de PLOMO en tu equipo.%n%nEl modpack y los archivos de Minecraft se descargan desde el propio launcher al pulsar "Descargar".%n%nSe recomienda cerrar los demás programas antes de continuar.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\PLOMO"; Filename: "{app}\PLOMO.exe"
Name: "{autodesktop}\PLOMO"; Filename: "{app}\PLOMO.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PLOMO.exe"; Description: "{cm:LaunchProgram,PLOMO}"; Flags: nowait postinstall skipifsilent
; Update started from inside the launcher (it runs this installer with /VERYSILENT /PLOMOUPDATE=1): open PLOMO again.
Filename: "{app}\PLOMO.exe"; Flags: nowait; Check: IsPlomoUpdate

[Code]
function IsPlomoUpdate: Boolean;
begin
  Result := ExpandConstant('{param:PLOMOUPDATE|0}') = '1';
end;

// El desinstalador solo quita lo que instalo el setup. Lo que el launcher creo despues (modpack, cuentas, mundos) se conserva
// salvo que el jugador lo pida de forma expresa.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and (not UninstallSilent) and DirExists(ExpandConstant('{app}')) then
    if MsgBox('¿Quieres borrar también los datos del juego?' + #13#10 + #13#10 +
              'Incluye el modpack instalado, las cuentas, la configuración y los mundos guardados. Esta acción no se puede deshacer.',
              mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      DelTree(ExpandConstant('{app}'), True, True, True);
end;
