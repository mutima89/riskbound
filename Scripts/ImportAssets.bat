@echo off
rem Imports Data/Assets (fetched by Scripts\FetchAssets.py) into Content/Riskbound, headless.
rem Usage: ImportAssets.bat "C:\Program Files\Epic Games\UE_5.8"
setlocal
if "%~1"=="" (echo Usage: %~nx0 ^<UE install dir^> & exit /b 2)
set ROOT=%~dp0..
"%~1\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%\Riskbound.uproject" -run=pythonscript -script="%ROOT%\Scripts\ImportAssets.py" -unattended -nopause -log=RbImport.log
set RESULT=%ERRORLEVEL%
findstr /C:"RbImport:" "%ROOT%\Saved\Logs\RbImport.log"
exit /b %RESULT%
