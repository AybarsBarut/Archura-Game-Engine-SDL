@echo off
echo ========================================
echo Building Archura Engine + Server
echo ========================================
echo.

REM Configure CMake
echo [1/3] Configuring CMake...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Build both executables
echo.
echo [2/3] Building Client + Server (Release)...
cmake --build build --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [3/3] Build complete!
echo.
echo Executables:
echo   - Client: build\bin\Release\Release\ArchuraEngine.exe
echo   - Server: build\bin\Release\Release\ArchuraServer.exe
echo.
pause
