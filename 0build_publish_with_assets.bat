@echo off
setlocal EnableExtensions EnableDelayedExpansion

set CONFIG=Release
set PUBLISH_SINGLE_FILE=true
set SELF_CONTAINED=true
set TRIM_UNUSED=false
set OUT_ROOT=out\publish
set PREF_CSPROJ=%~n1.csproj

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set DATESTAMP=%%i

set "CSPROJ="
if exist "%PREF_CSPROJ%" set "CSPROJ=%PREF_CSPROJ%"
if not defined CSPROJ (
  for %%f in (*.csproj) do (
    if not defined CSPROJ set "CSPROJ=%%f"
  )
)
if not defined CSPROJ call :die "No .csproj found"

where dotnet >nul 2>&1
if errorlevel 1 call :die "'dotnet' not found"

set "OUT64=%OUT_ROOT%\win-x64"
set "OUT86=%OUT_ROOT%\win-x86"
if exist "%OUT64%" rmdir /s /q "%OUT64%"
if exist "%OUT86%" rmdir /s /q "%OUT86%"
mkdir "%OUT64%" 2>nul
mkdir "%OUT86%" 2>nul

dotnet restore "%CSPROJ%" || call :die "restore failed"

dotnet publish "%CSPROJ%" -c %CONFIG% -r win-x64 -o "%OUT64%" ^
  -p:PublishSingleFile=%PUBLISH_SINGLE_FILE% ^
  -p:SelfContained=%SELF_CONTAINED% ^
  -p:PublishTrimmed=%TRIM_UNUSED% ^
  -p:EnableCompressionInSingleFile=true || call :die "publish win-x64 failed"

dotnet publish "%CSPROJ%" -c %CONFIG% -r win-x86 -o "%OUT86%" ^
  -p:PublishSingleFile=%PUBLISH_SINGLE_FILE% ^
  -p:SelfContained=%SELF_CONTAINED% ^
  -p:PublishTrimmed=%TRIM_UNUSED% ^
  -p:EnableCompressionInSingleFile=true || call :die "publish win-x86 failed"

if exist "Database" (
  robocopy "Database" "%OUT64%" /E
  robocopy "Database" "%OUT86%" /E
)

echo Done. Press any key to open the output folder...
pause >nul
start "" "%OUT_ROOT%"
exit /b 0

:die
echo.
echo [ERROR] %~1
echo Press any key to exit...
pause >nul
exit /b 1
