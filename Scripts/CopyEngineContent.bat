@echo off
rem Copies the UE mannequin (Priya Okafor's fallback body) from the local engine install into
rem Content/Characters/Mannequins. Epic template content is not stored in the public repository.
rem Usage: CopyEngineContent.bat "C:\Program Files\Epic Games\UE_5.8"
setlocal
if "%~1"=="" (echo Usage: %~nx0 ^<UE install dir^> & exit /b 2)
set SRC=%~1\Templates\TemplateResources\High\Characters\Content\Mannequins
set DST=%~dp0..\Content\Characters\Mannequins
for %%D in (Meshes Materials Textures Rigs) do robocopy "%SRC%\%%D" "%DST%\%%D" /E /NFL /NDL /NJH /NJS >nul
robocopy "%SRC%\Anims\Unarmed" "%DST%\Anims\Unarmed" MM_Idle.uasset /NFL /NDL /NJH /NJS >nul
if not exist "%DST%\Meshes\SKM_Quinn_Simple.uasset" (echo Mannequin copy failed & exit /b 1)
echo Mannequin copied to %DST%
exit /b 0
