@echo off
echo ========================================
echo Archura Dedicated Server Launcher
echo ========================================
echo.

REM Check if config file exists
if not exist "server_config.json" (
    echo [WARNING] server_config.json not found!
    echo Creating default configuration...
    echo.
)

REM Launch server
echo Starting Archura Dedicated Server...
echo.

ArchuraServer.exe --config server_config.json --verbose

echo.
echo Server stopped.
pause
