@echo off
rem Genera INSTALADOR\output\PLOMO-Setup-<version>.exe
rem 1) Vuelve a crear la carpeta limpia del launcher (LAUNCHER\work\portable-clean) con el PLOMO.exe que haya en LAUNCHER\portable.
rem 2) La compila con Inno Setup 6.
rem Antes, si cambiaste el codigo del launcher: LAUNCHER\tools\build_launcher.bat y copia Release\PLOMO.exe a LAUNCHER\portable.

"%~dp0..\.tools\vcpkg\downloads\tools\python\python-3.14.7-x64\python.exe" "%~dp0..\tools\package_preview.py" || goto :error

set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo No se encontro Inno Setup 6. Instalalo con: winget install JRSoftware.InnoSetup
  goto :error
)

"%ISCC%" "%~dp0plomo.iss" || goto :error
echo.
echo Instalador listo en %~dp0output
pause
exit /b 0

:error
echo.
echo Algo fallo; revisa los mensajes de arriba.
pause
exit /b 1
