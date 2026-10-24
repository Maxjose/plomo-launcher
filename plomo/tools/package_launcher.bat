@echo off
rem Builds LAUNCHER\PLOMO-<version>.zip from the program files in LAUNCHER\portable (no accounts, settings or game data).
rem Run tools\build_launcher.bat first if the launcher code changed, then copy Release\PLOMO.exe into portable\.
"%~dp0..\.tools\vcpkg\downloads\tools\python\python-3.14.7-x64\python.exe" "%~dp0package_preview.py"
echo.
pause
