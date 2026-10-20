@echo off
rem Packages Riskbound for Win64 (Shipping) with the engine's RunUAT.
rem Usage: BuildWindows.bat "C:\Program Files\Epic Games\UE_5.8"
rem NOT YET VERIFIED: first run must happen on a Windows PC with UE5 installed.
setlocal
if "%~1"=="" (echo Usage: %~nx0 ^<UE install dir^> & exit /b 2)
set UE=%~1
set ROOT=%~dp0..
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%ROOT%\Riskbound.uproject" -noP4 -platform=Win64 -clientconfig=Shipping ^
  -build -cook -stage -pak -archive -archivedirectory="%ROOT%\Release" -utf8output
exit /b %ERRORLEVEL%
