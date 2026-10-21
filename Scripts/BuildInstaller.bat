@echo off
rem Builds the Windows installer and the portable zip from the Shipping package (Inno Setup 6).
rem Usage: BuildInstaller.bat <version> [numeric version]   e.g. BuildInstaller.bat 0.5.0-alpha 0.5.0.0
rem Needs: Release\Shipping\Windows (Scripts\BuildWindows.bat <UE dir> Shipping) and Inno Setup 6.
rem Output: Release\Installer\Riskbound-Setup-<version>.exe (+ .sha256), Release\Riskbound-<version>-Win64-Portable.zip (+ .sha256)
setlocal
set ROOT=%~dp0..
if "%~1"=="" (echo Usage: %~nx0 ^<version^> [numeric version] & exit /b 2)
set VERSION=%~1
set NUMERIC=%~2
if "%NUMERIC%"=="" set NUMERIC=0.0.0.0
set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (echo Inno Setup 6 not found. Install it from https://jrsoftware.org/isdl.php & exit /b 3)
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Scripts\MakePortableZip.ps1" -Version %VERSION% || exit /b 1
"%ISCC%" /Q /DAppVersion=%VERSION% /DNumericVersion=%NUMERIC% "/DSourceDir=%ROOT%\Release\Portable\Riskbound-%VERSION%-Win64-Portable" "%ROOT%\Installer\Riskbound.iss" || exit /b 1
powershell -NoProfile -Command "$f='%ROOT%\Release\Installer\Riskbound-Setup-%VERSION%.exe'; $h=(Get-FileHash -Algorithm SHA256 $f).Hash.ToLower(); \"$h  Riskbound-Setup-%VERSION%.exe\" | Set-Content -Encoding ascii ($f+'.sha256'); Write-Output ('Installer: '+$f+' ('+[math]::Round((Get-Item $f).Length/1MB,1)+' MB)'); Write-Output ('SHA-256: '+$h)"
exit /b 0
