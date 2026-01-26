@echo off
echo ========================================
echo Archura Engine - Developer Build
echo ========================================
echo.

REM Configure CMake
echo [1/3] Configuring CMake...
cmake -S . -B build -DUSE_PREBUILT_GLFW=ON -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Build the project
echo.
echo [2/3] Building project (Release)...
cmake --build build --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Launch the game
echo.
echo [3/3] Launching Archura Engine...
if exist "build\bin\Release\Release\ArchuraEngine.exe" (
    echo Syncing Assets...
    xcopy assets build\bin\Release\Release\assets /E /I /Y
    cd build\bin\Release\Release
    mkdir logs 2>nul
    ArchuraEngine.exe --console
    echo Exit Code: %ERRORLEVEL%
    pause
) else (
    echo [ERROR] Executable not found! Build may have failed.
    pause
)

echo for bug fixes open a ticket from github repo!
echo do not close this window 