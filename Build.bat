@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows\engine.ps1" -Action Build %*
exit /b %ERRORLEVEL%
