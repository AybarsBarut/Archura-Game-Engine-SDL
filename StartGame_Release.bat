@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  Archura Engine  -  Release Baslatic + Arsiv Secici
REM ============================================================

cls
echo.
echo  ==========================================
echo    Archura Engine  ^|  Release Modu
echo  ==========================================
echo.

set ARCHIVE_DIR=builds\release
set LIVE_EXE=build\bin\Release\ArchuraEngine.exe

echo  Kayitli Release Derlemeler  ^(builds/release^):
echo  ------------------------------------------
echo.

set COUNT=0
set "FOUND_ANY=0"

for /f "tokens=*" %%F in ('dir /b /o:-d "%ARCHIVE_DIR%\ArchuraEngine_*.exe" 2^>nul') do (
    set /a COUNT+=1
    set "FILE_!COUNT!=%%F"
    set "FOUND_ANY=1"

    set "META_FILE=%ARCHIVE_DIR%\%%~nF.meta"
    set "META_VER=?"
    set "META_DATE=?"
    set "META_TIME=?"

    if exist "!META_FILE!" (
        for /f "tokens=1,2 delims==" %%K in (!META_FILE!) do (
            if "%%K"=="version" set "META_VER=%%L"
            if "%%K"=="date"    set "META_DATE=%%L"
            if "%%K"=="time"    set "META_TIME=%%L"
        )
    )

    for %%S in ("%ARCHIVE_DIR%\%%F") do set /a SIZE_KB=%%~zS/1024

    echo   [!COUNT!]  !META_DATE! !META_TIME!  v!META_VER!  !SIZE_KB! KB
    echo         !ARCHIVE_DIR!\%%F
    echo.
)

if "!FOUND_ANY!"=="0" (
    echo   Kayitli Release derlemesi bulunamadi.
    echo   Once Build.bat calistirin.
    echo.
    pause
    exit /b 1
)

echo   [0]  Aktif EXE: build\bin\Release\ArchuraEngine.exe
echo   [Q]  Cikis
echo.
set /p CHOICE=  Hangi surumu calistirmak istiyorsunuz? 

if /i "%CHOICE%"=="Q" exit /b 0

if "%CHOICE%"=="0" (
    set CHOSEN_PATH=%LIVE_EXE%
    goto LAUNCH
)

if %CHOICE% LSS 1 goto :EOF
if %CHOICE% GTR %COUNT% goto :EOF

set "CHOSEN_FILE=!FILE_%CHOICE%!"
set "CHOSEN_PATH=%ARCHIVE_DIR%\!CHOSEN_FILE!"
echo  Secilen: !CHOSEN_FILE!
copy /Y "!CHOSEN_PATH!" "%LIVE_EXE%" >nul

:LAUNCH
if not exist "!CHOSEN_PATH!" (
    echo  [HATA] EXE bulunamadi: !CHOSEN_PATH!
    pause
    exit /b 1
)

echo.
echo  Grafik API:
echo    [1] Otomatik ^(onerilen^)
echo    [2] Vulkan ^(bu build'de yoksa OpenGL'e geri doner^)
echo    [3] OpenGL ^(uyumluluk^)
set /p GRAPHICS_CHOICE=  Seciminiz [1]:
if "!GRAPHICS_CHOICE!"=="" set GRAPHICS_CHOICE=1
if "!GRAPHICS_CHOICE!"=="2" (
    set ARCHURA_GRAPHICS_CHOICE=vulkan
) else if "!GRAPHICS_CHOICE!"=="3" (
    set ARCHURA_GRAPHICS_CHOICE=opengl
) else (
    set ARCHURA_GRAPHICS_CHOICE=auto
)

echo  Assets senkronize ediliyor...
xcopy assets build\bin\Release\assets /E /I /Y /Q >nul

echo.
echo  ------------------------------------------
echo    Calistiriliyor...
echo  ------------------------------------------
cd build\bin\Release
ArchuraEngine.exe --graphics=!ARCHURA_GRAPHICS_CHOICE!
cd ..\..\..

echo.
echo  Motor kapatildi.
pause
endlocal
