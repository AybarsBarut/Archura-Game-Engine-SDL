@echo off
echo Publishing Release...
set BUILD_DIR=build\bin\Release\Release
set DIST_DIR=bin\Release_Dist

if not exist "%BUILD_DIR%\ArchuraEngine.exe" (
    echo [ERROR] Build not found at %BUILD_DIR%!
    echo Please run Build.bat first.
    pause
    exit /b 1
)

if exist "%DIST_DIR%" (
    echo Cleaning old distribution...
    rmdir /s /q "%DIST_DIR%"
)
mkdir "%DIST_DIR%"

echo Copying binary...
copy "%BUILD_DIR%\ArchuraEngine.exe" "%DIST_DIR%\"
if exist "%BUILD_DIR%\SDL2.dll" copy "%BUILD_DIR%\SDL2.dll" "%DIST_DIR%\"
if exist "%BUILD_DIR%\imgui.ini" copy "%BUILD_DIR%\imgui.ini" "%DIST_DIR%\"

echo Copying assets...
echo Copying assets...
xcopy "assets" "%DIST_DIR%\assets\" /E /I /Y

echo.
echo [SUCCESS] Release published to %DIST_DIR%
pause
