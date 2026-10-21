# Runs the two-phase UI walkthrough (Docs/WalkthroughReport.md) against a build.
#   -Exe   : packaged Riskbound.exe (e.g. Release\Shipping\Windows\Riskbound.exe); omit to use the editor in -game mode
#   -UE    : UE install dir (editor mode only)
#   -Shipping : the exe is a Shipping build in a folder not named Shipping (saves go to %LOCALAPPDATA%)
# Phase 1 plays from a new game and quits through the pause menu; phase 2 is a new process that uses Continue.
# Results: <Saved>\RbWalkthrough_1.txt, RbWalkthrough_2.txt and Saved\Screenshots\<platform>\Walk_*.png.
# The driver only injects input while the game window is the active application.
param(
    [string]$Exe = "",
    [string]$UE = "C:\Program Files\Epic Games\UE_5.8",
    [int]$TimeoutMinutes = 40,
    [switch]$Shipping
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if ($Exe -ne "") {
    $Saved = if ($Shipping -or $Exe -match "Shipping") { Join-Path $env:LOCALAPPDATA "Riskbound\Saved" } else { Join-Path (Split-Path -Parent $Exe) "Riskbound\Saved" }
    $Launch = { param($Mode) Start-Process -PassThru $Exe -ArgumentList "-RbSelfTest=$Mode", "-windowed", "-ResX=1920", "-ResY=1080" }
} else {
    $Saved = Join-Path $Root "Saved"
    $Launch = { param($Mode) Start-Process -PassThru "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$Root\Riskbound.uproject`"", "-game", "-windowed", "-ResX=1920", "-ResY=1080", "-nosplash", "-RbSelfTest=$Mode" }
}
Remove-Item "$Saved\RbWalkthrough_*.txt", "$Saved\SaveGames\walkthrough.rbsave*" -ErrorAction SilentlyContinue
Get-ChildItem "$Saved\Screenshots" -Recurse -Filter "Walk_*" -ErrorAction SilentlyContinue | Remove-Item
foreach ($Phase in @("walkthrough", "walkthrough2")) {
    $P = & $Launch $Phase
    if (-not $P.WaitForExit($TimeoutMinutes * 60000)) { Write-Output "$Phase timed out; stopping it"; $P.Kill() }
    $N = if ($Phase -eq "walkthrough") { 1 } else { 2 }
    $Log = "$Saved\RbWalkthrough_$N.txt"
    if (Test-Path $Log) { Get-Content $Log -TotalCount 4 | ForEach-Object { "phase ${N}: $_" } } else { "phase ${N}: no log written" }
}
Write-Output "Saved: $Saved"
