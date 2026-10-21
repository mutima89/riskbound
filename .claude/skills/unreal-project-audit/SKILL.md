---
name: unreal-project-audit
description: Audit the Riskbound Unreal toolchain (engine version, compiler, MCP, build and packaging readiness) and write Docs/EnvironmentAudit.md. Use at session start on a new machine or when a UE build or package fails for environment reasons.
---

# Unreal project audit

Produce a short, factual environment report. Record only what commands actually returned.

## Steps
1. OS and shell: `ver` / `uname -a`. Riskbound builds only on Windows x64.
2. Engine: find installs (`C:\Program Files\Epic Games\UE_*`, or `HKLM\SOFTWARE\EpicGames\Unreal Engine`). Read `Engine\Build\Build.version` for the exact version. Compare with `EngineAssociation` in `Riskbound.uproject`.
3. Compiler: `vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`. UE5 needs VS2022 with the C++ game workload and a Windows 10/11 SDK.
4. MCP: list configured MCP servers and their tools. Only report an Unreal MCP as available if a tool call against it succeeds. Otherwise the workflow is UBT/RunUAT from the command line.
5. Build probe: `"<UE>\Engine\Build\BatchFiles\Build.bat" RiskboundEditor Win64 Development -project="<repo>\Riskbound.uproject" -waitmutex`. Capture the last 30 lines on failure.
6. Packaging probe (only if 5 passed): `Scripts\BuildWindows.bat "<UE>"`.
7. Installer tools: check for Inno Setup (`ISCC.exe`), NSIS (`makensis`), WiX. Signing: `signtool` plus a certificate.
8. Core tests still pass: `Scripts\RunCoreTests.bat`.

## Output
Rewrite `Docs/EnvironmentAudit.md` (findings table, blockers, workaround) and add one line to `Docs/Progress.md`. Do not paste full logs; quote the decisive lines.
