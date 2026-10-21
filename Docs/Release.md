# Releasing Riskbound (Windows)

Current release: **0.5.1-alpha** (notes: `Docs/ReleaseNotes-0.5.1-alpha.md`; previous: `Docs/ReleaseNotes-0.5.0-alpha.md`, known issues: `Docs/KnownIssues.md`).

## Outputs
| File | What |
|---|---|
| `Release\Installer\Riskbound-Setup-<version>.exe` | Inno Setup 6 installer: per-user install by default (no admin rights), all-users if an administrator chooses; Start-menu shortcuts (game, read me, licences, uninstall); optional desktop shortcut; licence and credits page; unsigned-build notice page; uninstaller that keeps saves |
| `Release\Riskbound-<version>-Win64-Portable.zip` | Same files without an installer: unzip and run |
| `*.sha256` next to each | SHA-256 to publish alongside the downloads |

Both are built from one staged folder (`Release\Portable\Riskbound-<version>-Win64-Portable`), so they ship identical files: the Shipping package minus debug symbols (.pdb) and packaging manifests, plus `README.txt`, `UNSIGNED-BUILD.txt` and `Licences\` (code MIT, asset credits, Inter OFL, Unreal Engine notice). Sources for those texts: `Docs/Release/`.

## Steps
1. Gates green on this commit: `Scripts\RunCoreTests.bat`, `Scripts\RunUnrealTests.bat <UE>` (record in Docs/QAReport.md).
2. `Scripts\BuildWindows.bat "C:\Program Files\Epic Games\UE_5.8" Shipping`
3. Packaged self-tests on Shipping (`Release\Shipping\Windows\Riskbound.exe`): `-RbSelfTest=play`, then `-RbSelfTest=resume`, then `-RbSelfTest=tour -ResX=1920 -ResY=1080`. Results land in `%LOCALAPPDATA%\Riskbound\Saved\` (`RbSelfTest_play.txt`, `RbSelfTest.txt`, `RbTour.txt`).
4. `Scripts\BuildInstaller.bat <version> <numeric version>`, e.g. `Scripts\BuildInstaller.bat 0.5.1-alpha 0.5.1.0` (needs Inno Setup 6). This also builds the portable zip.
5. Install test (silent, to a scratch folder, current user):
   ```
   Riskbound-Setup-<version>.exe /VERYSILENT /SUPPRESSMSGBOXES /CURRENTUSER /NORESTART /DIR="%LOCALAPPDATA%\RiskboundInstallTest" /TASKS=desktopicon
   ```
   Check the files, the Start-menu folder and desktop shortcut, the uninstall entry (HKCU ...\Uninstall\{6F1D4B2A-8C3E-4F57-9A0B-52C7E1D3A9F4}_is1). Run `-RbSelfTest=play` and `-RbSelfTest=resume` from the installed folder. Uninstall with `unins000.exe /VERYSILENT /SUPPRESSMSGBOXES`; check the folder, shortcuts and uninstall entry are gone and `%LOCALAPPDATA%\Riskbound\Saved\SaveGames` is still there.
6. Zip test: extract the zip to a new folder and run `-RbSelfTest=play` and `resume` from it.
7. Record everything in Docs/QAReport.md, update the release notes and known issues.
8. When the owner approves: merge to main, then `gh release create v<version> --target main --prerelease --title "Riskbound <version>" --notes-file Docs/ReleaseNotes-<version>.md` with the installer, the zip and both `.sha256` files attached (GitHub allows up to 2 GB per file).

## Signing
Builds are not code-signed: Windows SmartScreen shows "Unknown publisher" until a certificate is bought and `SignTool` is added to step 4. Until then, publish the SHA-256 files and point players to `UNSIGNED-BUILD.txt`.

## Tools
Inno Setup 6.7.3 (installed per user with `winget install JRSoftware.InnoSetup`, 2026-10-21, approved by the owner). Nothing else is installed for releases.
