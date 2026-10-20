@echo off
rem Creates Content/Maps/L_MeridianHQ.umap with the editor's Python plugin (opens and closes the editor).
rem Usage: CreateMap.bat "C:\Program Files\Epic Games\UE_5.8"
setlocal
if "%~1"=="" (echo Usage: %~nx0 ^<UE install dir^> & exit /b 2)
set ROOT=%~dp0..
"%~1\Engine\Binaries\Win64\UnrealEditor.exe" "%ROOT%\Riskbound.uproject" -ExecutePythonScript="%ROOT%\Scripts\CreateMeridianHQMap.py" -nosplash -log -unattended
exit /b %ERRORLEVEL%
