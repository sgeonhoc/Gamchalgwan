@echo off
setlocal EnableExtensions
rem ===================================================================
rem  Gamchalgwan (dock graybox) - one-click Win64 build
rem  Based on LAUNCH:ER Day 8 build_unreal.bat
rem  Usage   : tools\build_unreal.bat 0.1.0   (or double-click and type a version)
rem  Output  : %OUT_ROOT%\Gamchalgwan_v<version>_<date>.zip  (+ .log)
rem  Steps   : 0) build editor module (the cooker loads it)
rem            1) BuildCookRun Shipping  2) add README_PLAY.txt  3) zip
rem  -nodebuginfo keeps the .pdb (about 200 MB) out of the zip; players never need it.
rem  -prereqs stages Engine\Extras\Redist\en-us\vc_redist.x64.exe for PCs without the VC++ runtime.
rem  Comments are in English on purpose (bat files + code pages).
rem ===================================================================

rem ---- 1) Change these lines for your PC (env vars override) --------
if not defined UE_ROOT  set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
if not defined PROJECT  set "PROJECT=%~dp0..\Gamchalgwan.uproject"
if not defined OUT_ROOT set "OUT_ROOT=C:\LE3\Builds"
if not defined CONFIG   set "CONFIG=Shipping"
rem  Lower this on a busy PC (compiler jobs at the same time).
if not defined MAX_JOBS set "MAX_JOBS=4"
rem -------------------------------------------------------------------

set "RC=1"
set "ASKED="
set "VERSION=%~1"
if "%VERSION%"=="" (
  set "ASKED=1"
  set /p "VERSION=Version (e.g. 0.1.0): "
)
if "%VERSION%"=="" goto :usage
set "VERSION=%VERSION: =%"

set "UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"
set "UBT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
if not exist "%UAT%" goto :err_engine
if not exist "%PROJECT%" goto :err_project
for %%P in ("%PROJECT%") do set "PROJECT=%%~fP"
for %%P in ("%PROJECT%") do set "GAME=%%~nP"

for /f "delims=" %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmm"') do set "STAMP=%%T"
set "NAME=%GAME%_v%VERSION%_%STAMP%"
set "OUT=%OUT_ROOT%\%NAME%"
set "LOG=%OUT_ROOT%\%NAME%.log"
if not exist "%OUT_ROOT%" mkdir "%OUT_ROOT%"

echo [0/3] Building editor module (needed by the cooker)
call "%UBT%" %GAME%Editor Win64 Development -Project="%PROJECT%" -WaitMutex -MaxParallelActions=%MAX_JOBS% > "%LOG%" 2>&1
if errorlevel 1 goto :err_build

echo [1/3] Packaging %GAME% v%VERSION% [%CONFIG%, Win64]
echo       This can take 5-30 minutes. Live log: %LOG%
call "%UAT%" BuildCookRun ^
  -project="%PROJECT%" -platform=Win64 -clientconfig=%CONFIG% ^
  -nop4 -utf8output -unattended -installed ^
  -nocompileeditor -skipbuildeditor ^
  -ubtargs="-MaxParallelActions=%MAX_JOBS%" ^
  -build -cook -stage -package -pak -iostore -compressed -prereqs -nodebuginfo ^
  -archive -archivedirectory="%OUT%" >> "%LOG%" 2>&1
if errorlevel 1 goto :err_build

echo [2/3] Adding README_PLAY.txt
if exist "%~dp0README_PLAY.txt" copy /y "%~dp0README_PLAY.txt" "%OUT%\README_PLAY.txt" > nul

echo [3/3] Zipping
if exist "%OUT%.zip" del "%OUT%.zip"
tar -a -c -f "%OUT%.zip" -C "%OUT%" .
if errorlevel 1 goto :err_zip

echo Done: %OUT%.zip
echo Next: add a line to CHANGELOG.md and run  git tag v%VERSION%
set "RC=0"
goto :finish

:usage
echo Usage: %~nx0 ^<version^>   e.g. %~nx0 0.1.0
set "RC=2"
goto :finish

:err_engine
echo [ERROR] RunUAT.bat not found. Check UE_ROOT: %UE_ROOT%
goto :finish

:err_project
echo [ERROR] .uproject not found. Check PROJECT: %PROJECT%
goto :finish

:err_build
echo [FAILED] Open the log and search for the FIRST line with "Error":
echo          %LOG%
goto :finish

:err_zip
echo [FAILED] Could not create %OUT%.zip
goto :finish

:finish
if defined ASKED pause
exit /b %RC%
