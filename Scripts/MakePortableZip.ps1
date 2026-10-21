# Builds a portable (no installer) release zip from an existing packaged build.
# Usage: powershell -ExecutionPolicy Bypass -File Scripts\MakePortableZip.ps1 [-Config Shipping] [-Version 0.5.0]
# Output: Release\Riskbound-<Version>-Win64-Portable.zip and a .sha256 file next to it.
# Installs nothing. Debug symbols (.pdb) and packaging manifests are left out.
param(
    [string]$Config = "Shipping",
    [string]$Version = ""
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "Release\$Config\Windows"
if (-not (Test-Path (Join-Path $Build "Riskbound.exe"))) { throw "No packaged build at $Build. Run Scripts\BuildWindows.bat first." }
if ($Version -eq "") {
    $Sha = (git -C $Root rev-parse --short HEAD).Trim()
    $Version = (Get-Date -Format "yyyy.MM.dd") + "-" + $Sha
}
$Name = "Riskbound-$Version-Win64-Portable"
$Stage = Join-Path $Root "Release\Portable\$Name"
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force $Stage | Out-Null

# Game files, without debug symbols and manifests.
Get-ChildItem -Path $Build -Recurse -File | Where-Object {
    $_.Extension -ne ".pdb" -and $_.Name -notlike "Manifest_*.txt" -and $_.FullName -notmatch "\\Saved\\"
} | ForEach-Object {
    $Rel = $_.FullName.Substring($Build.Length + 1)
    $Dest = Join-Path $Stage $Rel
    New-Item -ItemType Directory -Force (Split-Path -Parent $Dest) | Out-Null
    Copy-Item $_.FullName $Dest
}

# Documents and licences.
$Docs = Join-Path $Root "Docs\Release"
Copy-Item (Join-Path $Docs "README.txt") (Join-Path $Stage "README.txt")
Copy-Item (Join-Path $Docs "UNSIGNED-BUILD.txt") (Join-Path $Stage "UNSIGNED-BUILD.txt")
$Lic = Join-Path $Stage "Licences"
New-Item -ItemType Directory -Force $Lic | Out-Null
Copy-Item (Join-Path $Root "LICENSE") (Join-Path $Lic "Riskbound-code-MIT.txt")
Copy-Item (Join-Path $Root "Docs\AssetCredits.md") (Join-Path $Lic "AssetCredits.md")
Copy-Item (Join-Path $Root "Resources\Fonts\OFL.txt") (Join-Path $Lic "Inter-font-OFL.txt")
Copy-Item (Join-Path $Docs "UnrealEngine-Notice.txt") (Join-Path $Lic "UnrealEngine-Notice.txt")
(Get-Content (Join-Path $Stage "README.txt")) -replace "\{VERSION\}", $Version | Set-Content -Encoding utf8 (Join-Path $Stage "README.txt")

$Zip = Join-Path $Root "Release\$Name.zip"
if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path $Stage -DestinationPath $Zip -CompressionLevel Optimal
$Hash = (Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLower()
"$Hash  $Name.zip" | Set-Content -Encoding ascii "$Zip.sha256"
$Size = [math]::Round((Get-Item $Zip).Length / 1MB, 1)
Write-Output "Portable zip: $Zip ($Size MB)"
Write-Output "SHA-256: $Hash"
