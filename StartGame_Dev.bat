@echo off
setlocal
echo ========================================
echo  Archura Engine - Developer Build (Debug)
echo ========================================
echo.

REM Configure CMake (no CMAKE_BUILD_TYPE for multi-config generators like MSVC)
echo [1/3] Configuring CMake...
cmake -S . -B build -DUSE_PREBUILT_GLFW=ON
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Build only the engine target in Debug config
echo.
echo [2/3] Building ArchuraEngine (Debug)...
cmake --build build --config Debug --target ArchuraEngine
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Launch the game from the correct output directory
echo.
echo [3/3] Launching Archura Engine (Debug)...
if exist "build\bin\Debug\ArchuraEngine.exe" (
    echo Syncing Assets...
    xcopy assets build\bin\Debug\assets /E /I /Y /Q
    cd build\bin\Debug
    if not exist logs mkdir logs
    ArchuraEngine.exe --console
    echo.
    echo Exit Code: %ERRORLEVEL%
    cd ..\..\..
    pause
) else (
    echo [ERROR] Executable not found: build\bin\Debug\ArchuraEngine.exe
    echo Make sure the build succeeded and CMake is configured correctly.
    pause
)