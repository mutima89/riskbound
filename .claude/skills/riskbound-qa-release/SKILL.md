---
name: riskbound-qa-release
description: Run Riskbound release gates - targeted and full tests, Windows build, packaging, packaged-exe and offline checks, save/backup verification, installer, and the QA report. Use at the end of a gate or before handing over a build.
---

# Riskbound QA and release

Strategy: `Docs/TestingStrategy.md`. Report: `Docs/QAReport.md`. Requires Windows + UE5 for steps 2-7.

## Steps
1. Core: `Scripts\RunCoreTests.bat` (or `.sh`). Record counts.
2. UE automation: `Scripts\RunUnrealTests.bat "<UE dir>"`. Record pass/fail from the log summary.
3. Package: `Scripts\BuildWindows.bat "<UE dir>"`. Confirm `Release\Windows\Riskbound.exe` exists with `dir`; record size and timestamp.
4. Packaged smoke test, outside the editor, with networking disabled:
   - launches to main menu; new game starts as Junior Risk Analyst;
   - complete Mission 001; manual save; quit; relaunch; continue -> same objective, career level, competencies, register entry R-014;
   - corrupt the primary save (edit one byte), relaunch -> "recovered from backup" message and correct state.
5. Settings: text size, sensitivity, reduced motion, fullscreen/windowed persist across relaunch.
6. Installer (if Inno Setup/NSIS available): build `Riskbound_Setup.exe`, install to a clean folder, run, uninstall.
7. Signing: if no certificate, state "unsigned" in release notes.

## Reporting
Append a dated section to `Docs/QAReport.md`: each check, actual result, evidence (file path, log line). Failures and skipped checks are listed as such. Never mark a step passed without its evidence. Update `Docs/Progress.md` gate checklist.
