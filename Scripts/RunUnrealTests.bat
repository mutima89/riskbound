@echo off
rem Runs Unreal automation tests under the Riskbound. prefix, headless.
rem Usage: RunUnrealTests.bat "C:\Program Files\Epic Games\UE_5.5"
rem NOT YET VERIFIED: requires Gate 1 automation tests and a Windows UE5 install.
setlocal
if "%~1"=="" (echo Usage: %~nx0 ^<UE install dir^> & exit /b 2)
set ROOT=%~dp0..
"%~1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%\Riskbound.uproject" ^
  -ExecCmds="Automation RunTests Riskbound.;Quit" -unattended -nullrhi -nopause -log -ReportExportPath="%ROOT%\Saved\TestReport"
exit /b %ERRORLEVEL%
