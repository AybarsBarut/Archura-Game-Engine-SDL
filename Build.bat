@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  Archura Engine  -  Release Build + Arsiv Sistemi
REM ============================================================

echo.
echo  ==========================================
echo    Archura Engine  ^|  Release Build
echo  ==========================================
echo.

set /p VERSION=<version.txt
set BUILD_DATE=%DATE:~10,4%-%DATE:~7,2%-%DATE:~4,2%
set BUILD_TIME=%TIME:~0,2%-%TIME:~3,2%
set BUILD_TIME=%BUILD_TIME: =0%
set STAMP=v%VERSION%_%BUILD_DATE%_%BUILD_TIME%

set ARCHIVE_DIR=builds\release
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

echo  [1/3] CMake yapilandiriliyor...
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo  [INFO] CMake cache yenileniyor...
    cmake --fresh -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
)
if %ERRORLEVEL% NEQ 0 (
    echo  [HATA] CMake yapilandirma basarisiz!
    pause
    exit /b 1
)

echo.
echo  [2/3] Release derleniyor ^(%STAMP%^)...
cmake --build build --config Release --target ArchuraEngine
if %ERRORLEVEL% NEQ 0 (
    echo  [HATA] Derleme basarisiz!
    pause
    exit /b 1
)

set SRC_EXE=build\bin\Release\ArchuraEngine.exe
set ARCHIVE_EXE=%ARCHIVE_DIR%\ArchuraEngine_%STAMP%.exe

if not exist "%SRC_EXE%" (
    echo  [HATA] EXE bulunamadi: %SRC_EXE%
    pause
    exit /b 1
)

echo.
echo  [3/3] Arsivleniyor...
copy /Y "%SRC_EXE%" "%ARCHIVE_EXE%" >nul
if %ERRORLEVEL% NEQ 0 (
    echo  [UYARI] Arsive kopyalama basarisiz.
) else (
    echo  [OK] Arsivlendi: %ARCHIVE_EXE%
)

(
    echo version=%VERSION%
    echo date=%BUILD_DATE%
    echo time=%BUILD_TIME%
    echo config=Release
    echo stamp=%STAMP%
) > "%ARCHIVE_DIR%\ArchuraEngine_%STAMP%.meta"

set SRC_DIR=build\bin\Release
for %%D in (SDL2.dll SDL2_ttf.dll mono-2.0-sgen.dll) do (
    if exist "%SRC_DIR%\%%D" copy /Y "%SRC_DIR%\%%D" "%ARCHIVE_DIR%\" >nul
)

echo.
echo  ==========================================
echo    BUILD TAMAMLANDI
echo.
echo    Surum  : v%VERSION%
echo    Tarih  : %BUILD_DATE% %BUILD_TIME%
echo    Arsiv  : %ARCHIVE_EXE%
echo.
echo    Dagitim icin  : Publish_Release.bat
echo    Baslatmak icin: StartGame_Release.bat
echo  ==========================================
echo.
pause
