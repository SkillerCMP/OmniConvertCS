@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "FINISHED_DIR=%~dp0"
set "FINISHED_DIR=%FINISHED_DIR%0-Finished"

set "CONFIG=Release"
set "TOOLSET=auto"
set "CLEAN_ARG="
set "RESULT=1"

for %%A in (%*) do (
    if /I "%%~A"=="Debug" set "CONFIG=Debug"
    if /I "%%~A"=="Release" set "CONFIG=Release"
    if /I "%%~A"=="RelWithDebInfo" set "CONFIG=RelWithDebInfo"
    if /I "%%~A"=="MinSizeRel" set "CONFIG=MinSizeRel"
    if /I "%%~A"=="clean" set "CLEAN_ARG=-Clean"
    if /I "%%~A"=="auto" set "TOOLSET=auto"
    if /I "%%~A"=="v143" set "TOOLSET=v143"
    if /I "%%~A"=="v142" set "TOOLSET=v142"
    if /I "%%~A"=="v141" set "TOOLSET=v141"
    if /I "%%~A"=="help" goto :usage
    if /I "%%~A"=="/?" goto :usage
)

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: Windows PowerShell was not found.
    set "RESULT=1"
    goto :finish
)

echo Starting the visible Windows 7 x64 build...
echo Configuration: %CONFIG%
echo Toolset:      %TOOLSET%
echo Cleanup:      Remove the build folder when this command finishes
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
    -File "%~dp0tools\build-windows7-x64.ps1" ^
    -Configuration "%CONFIG%" ^
    -Toolset "%TOOLSET%" %CLEAN_ARG%
set "RESULT=%ERRORLEVEL%"

:finish
echo.
echo Removing temporary build folder...
if exist "%~dp0build\" (
    rmdir /s /q "%~dp0build"
    if exist "%~dp0build\" (
        echo WARNING: The build folder could not be completely removed.
    ) else (
        echo Removed: %~dp0build
    )
) else (
    echo Build folder is already absent.
)
echo Logs remain in: %FINISHED_DIR%\logs
echo Release files remain in: %FINISHED_DIR%\dist

echo.
if "%RESULT%"=="0" (
    echo Build completed successfully.
) else (
    echo Build failed with exit code %RESULT%.
)

if /I not "%CI%"=="true" pause
exit /b %RESULT%

:usage
echo.
echo Usage:
echo   build-windows7-x64.cmd [Release^|Debug^|RelWithDebInfo^|MinSizeRel] [clean] [auto^|v143^|v142^|v141]
echo.
echo Examples:
echo   build-windows7-x64.cmd
echo   build-windows7-x64.cmd Release clean
echo   build-windows7-x64.cmd Release clean v142
echo.
echo The temporary build folder is always removed after the command finishes,
echo whether the build succeeds or fails. Logs and releases are preserved under 0-Finished.
echo.
echo Auto searches every installed Visual Studio edition and Build Tools instance
echo for the newest Windows 7-compatible C++ toolset: v143, v142, or v141.
echo A Visual Basic-only installation does not include the required C++ compiler.
echo.
exit /b 0
