@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  Archura Engine  -  Developer Build + Arsiv Sistemi
REM ============================================================

set /p VERSION=<version.txt
set BUILD_DATE=%DATE:~10,4%-%DATE:~7,2%-%DATE:~4,2%
set BUILD_TIME=%TIME:~0,2%-%TIME:~3,2%
set BUILD_TIME=%BUILD_TIME: =0%
set STAMP=v%VERSION%_%BUILD_DATE%_%BUILD_TIME%

set SRC_EXE=build\bin\Debug\ArchuraEngine.exe
set ARCHIVE_DIR=builds\debug
if not exist "%ARCHIVE_DIR%" mkdir "%ARCHIVE_DIR%"

where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)

REM ============================================================
:MENU
cls
echo.
echo  ==========================================
echo    Archura Engine  ^|  Gelistirici Modu
echo  ==========================================
echo.
echo    [1]  Derle + Calistir (yeni Debug build)
echo    [2]  Kayitli Derlemeleri Listele ve Sec
echo    [3]  Sadece Derle (calistirma)
echo    [Q]  Cikis
echo.
set /p CHOICE=  Seciminiz: 

if /i "%CHOICE%"=="1" goto BUILD_AND_RUN
if /i "%CHOICE%"=="2" goto SELECT_BUILD
if /i "%CHOICE%"=="3" goto BUILD_ONLY
if /i "%CHOICE%"=="Q" goto END
goto MENU

REM ============================================================
:BUILD_AND_RUN
cls
echo.
echo  [1/3] CMake yapilandiriliyor...
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [INFO] CMake cache yenileniyor...
    cmake --fresh -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug >nul 2>&1
)
if %ERRORLEVEL% NEQ 0 (
    echo  [HATA] CMake yapilandirma basarisiz!
    pause & goto MENU
)

echo  [2/3] Debug derleniyor...
cmake --build build --config Debug --target ArchuraEngine
if %ERRORLEVEL% NEQ 0 (
    echo  [HATA] Derleme basarisiz!
    pause & goto MENU
)

set ARCHIVE_EXE=%ARCHIVE_DIR%\ArchuraEngine_%STAMP%.exe
echo.
echo  [3/3] Arsivleniyor: %ARCHIVE_EXE%
copy /Y "%SRC_EXE%" "%ARCHIVE_EXE%" >nul
(
    echo version=%VERSION%
    echo date=%BUILD_DATE%
    echo time=%BUILD_TIME%
    echo config=Debug
    echo stamp=%STAMP%
) > "%ARCHIVE_DIR%\ArchuraEngine_%STAMP%.meta"

echo  [OK] Arsivlendi. Calistiriliyor...
goto RUN_EXE_DIRECT

REM ============================================================
:BUILD_ONLY
cls
echo.
echo  [1/2] CMake yapilandiriliyor...
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [INFO] CMake cache yenileniyor...
    cmake --fresh -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug >nul 2>&1
)
if %ERRORLEVEL% NEQ 0 (
    echo  [HATA] CMake yapilandirma basarisiz!
    pause & goto MENU
)
echo  [2/2] Debug derleniyor...
cmake --build build --config Debug --target ArchuraEngine
if %ERRORLEVEL% NEQ 0 (
    echo  [HATA] Derleme basarisiz!
    pause & goto MENU
)

set ARCHIVE_EXE=%ARCHIVE_DIR%\ArchuraEngine_%STAMP%.exe
copy /Y "%SRC_EXE%" "%ARCHIVE_EXE%" >nul
(
    echo version=%VERSION%
    echo date=%BUILD_DATE%
    echo time=%BUILD_TIME%
    echo config=Debug
    echo stamp=%STAMP%
) > "%ARCHIVE_DIR%\ArchuraEngine_%STAMP%.meta"

echo.
echo  [SUCCESS] Derleme tamamlandi: %ARCHIVE_EXE%
pause & goto MENU

REM ============================================================
:SELECT_BUILD
cls
echo.
echo  ==========================================
echo    Kayitli Debug Derlemeler
echo  ==========================================
echo.

set COUNT=0
set "FOUND_ANY=0"

for /f "tokens=*" %%F in ('dir /b /o:d "%ARCHIVE_DIR%\ArchuraEngine_*.exe" 2^>nul') do (
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
    echo         %%F
    echo.
)

if "!FOUND_ANY!"=="0" (
    echo   Kayitli derleme bulunamadi.
    echo   Once [1] ile bir derleme yapin.
    echo.
    pause & goto MENU
)

echo   [0]  Aktif EXE'yi kullan ^(build\bin\Debug\ArchuraEngine.exe^)
echo   [M]  Ana Menuye Don
echo.
set /p BUILD_CHOICE=  Hangi surumu calistirmak istiyorsunuz? 

if /i "%BUILD_CHOICE%"=="M" goto MENU
if "%BUILD_CHOICE%"=="0" goto RUN_EXE_DIRECT

if %BUILD_CHOICE% LSS 1 goto SELECT_BUILD
if %BUILD_CHOICE% GTR %COUNT% goto SELECT_BUILD

set "CHOSEN_FILE=!FILE_%BUILD_CHOICE%!"
set "CHOSEN_PATH=%ARCHIVE_DIR%\!CHOSEN_FILE!"

echo.
echo  Secilen: !CHOSEN_FILE!
echo  Aktif konuma kopyalaniyor...
copy /Y "!CHOSEN_PATH!" "%SRC_EXE%" >nul

REM ============================================================
:RUN_EXE_DIRECT
if not exist "%SRC_EXE%" (
    echo  [HATA] EXE bulunamadi: %SRC_EXE%
    pause & goto MENU
)

call :SELECT_GRAPHICS_API

echo  Assets senkronize ediliyor...
xcopy assets build\bin\Debug\assets /E /I /Y /Q >nul

cd build\bin\Debug
if not exist logs mkdir logs
echo.
echo  ------------------------------------------
echo    Calistiriliyor (Debug + konsol)
echo  ------------------------------------------
ArchuraEngine.exe --console --graphics=%ARCHURA_GRAPHICS_CHOICE%
set EXIT_CODE=%ERRORLEVEL%
cd ..\..\..

echo.
echo  Motor kapatildi. Cikis kodu: %EXIT_CODE%
echo.
pause
goto MENU

:SELECT_GRAPHICS_API
echo.
echo  Grafik API:
echo    [1] Otomatik ^(onerilen^)
echo    [2] Vulkan ^(bu build'de yoksa OpenGL'e geri doner^)
echo    [3] OpenGL ^(uyumluluk^)
set /p GRAPHICS_CHOICE=  Seciminiz [1]:
if "%GRAPHICS_CHOICE%"=="" set GRAPHICS_CHOICE=1
if "%GRAPHICS_CHOICE%"=="2" set ARCHURA_GRAPHICS_CHOICE=vulkan& exit /b 0
if "%GRAPHICS_CHOICE%"=="3" set ARCHURA_GRAPHICS_CHOICE=opengl& exit /b 0
set ARCHURA_GRAPHICS_CHOICE=auto
exit /b 0

:END
endlocal
exit /b 0
