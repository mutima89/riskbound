@echo off
rem Packages Riskbound for Win64 with the engine's RunUAT.
rem Usage: BuildWindows.bat "C:\Program Files\Epic Games\UE_5.8" [Development|Shipping]
rem Output: Release\<Config>\Windows\Riskbound.exe
setlocal
if "%~1"=="" (echo Usage: %~nx0 ^<UE install dir^> [Development^|Shipping] & exit /b 2)
set UE=%~1
set CONFIG=%~2
if "%CONFIG%"=="" set CONFIG=Shipping
set ROOT=%~dp0..
rem Optional third argument: archive directory (default Release\<Config>), e.g. to keep a build someone is playing.
set ARCHIVE=%~3
if "%ARCHIVE%"=="" set ARCHIVE=%ROOT%\Release\%CONFIG%
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%ROOT%\Riskbound.uproject" -noP4 -platform=Win64 -clientconfig=%CONFIG% ^
  -build -cook -stage -pak -archive -archivedirectory="%ARCHIVE%" -utf8output
exit /b %ERRORLEVEL%
