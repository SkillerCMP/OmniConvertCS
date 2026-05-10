@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

rem ============================================================
rem  Simple single-csproj publish script
rem  Project target: .NET 10 / C# 14
rem  - Auto-detects the single .csproj in current root
rem  - Publishes SELF-CONTAINED + SINGLE-FILE
rem  - Copies only 0-Resources if present
rem ============================================================

set "RID=win-x64"
set "CONFIG=Release"
set "OUT_DIR=publish\%RID%"
set "COPY_RESOURCES=true"

set "PROJECT="
set /a PROJECT_COUNT=0

for %%F in (*.csproj) do (
    set /a PROJECT_COUNT+=1
    if !PROJECT_COUNT! EQU 1 set "PROJECT=%%~nxF"
)

if %PROJECT_COUNT% EQU 0 (
    echo ERROR: Could not find any .csproj in:
    echo %CD%
    pause
    exit /b 1
)

if %PROJECT_COUNT% GTR 1 (
    echo ERROR: Found more than one .csproj in:
    echo %CD%
    echo.
    for %%F in (*.csproj) do echo   - %%~nxF
    echo.
    echo This script expects only one .csproj in the root folder.
    pause
    exit /b 1
)

where dotnet >nul 2>&1
if errorlevel 1 (
    echo ERROR: dotnet was not found in PATH.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo   Simple Publish - Self Contained Single File
echo ==========================================
echo.
echo   Project : %PROJECT%
echo   Config  : %CONFIG%
echo   RID     : %RID%
echo   Output  : %OUT_DIR%
echo.
echo Installed .NET SDK:
dotnet --version
echo.

if exist "%OUT_DIR%" (
    echo Deleting previous publish folder...
    rmdir /s /q "%OUT_DIR%"
)

echo Restoring...
dotnet restore "%PROJECT%"
if errorlevel 1 (
    echo.
    echo RESTORE FAILED.
    pause
    exit /b 1
)

echo.
echo Publishing...
dotnet publish "%PROJECT%" ^
  -c "%CONFIG%" ^
  -r "%RID%" ^
  --self-contained true ^
  -p:PublishSingleFile=true ^
  -p:IncludeNativeLibrariesForSelfExtract=true ^
  -p:EnableCompressionInSingleFile=true ^
  -p:PublishTrimmed=false ^
  -o "%OUT_DIR%"

if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    pause
    exit /b 1
)

if /i "%COPY_RESOURCES%"=="true" (
    if exist "0-Resources" (
        echo.
        echo Copying 0-Resources...
        robocopy "0-Resources" "%OUT_DIR%\0-Resources" /E >nul
    )
)

if exist "bin" (
    echo.
    echo Deleting bin folder...
    rmdir /s /q "bin"
)

if exist "obj" (
    echo Deleting obj folder...
    rmdir /s /q "obj"
)

echo.
echo BUILD COMPLETE.
echo Output:
echo   %CD%\%OUT_DIR%
pause
exit /b 0
