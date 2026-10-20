# Environment audit (Gate 0)

Date: 2026-10-20. Session: Claude Code cloud container.

## Findings
| Check | Result |
|---|---|
| OS | Linux x86_64 (cloud container). Not Windows. |
| Unreal Engine | **Not installed.** No UE install, no UnrealBuildTool, no RunUAT. Cannot be installed here (Epic account + Windows/large download). |
| Unreal MCP integration | **None available.** No Unreal MCP server is configured in this session. |
| C++ toolchain | GCC 13.3, Clang 18.1, CMake 3.28, Ninja. Used for the simulation core. |
| MSVC / Windows SDK | Not available. UE5 on Windows needs Visual Studio 2022 with the "Game development with C++" workload. |
| Installer tooling | Not available (Inno Setup / NSIS / WiX need Windows). |
| Code signing | Not available. Releases will be unsigned unless a certificate is supplied. |
| GitHub repo `riskbound` | No repository attached to the session; none named `riskbound` visible to it. |
| User device / Remote Control | No device connected. |
| Claude Code skills | Project skill format: `.claude/skills/<name>/SKILL.md` with `name` + `description` frontmatter. Five project skills created. No installed skill covers Unreal. |
| Subagents | Available, not used (task boundaries do not justify them yet). |

## Blockers
1. **UE build, PIE testing, packaging, installer** - need a Windows PC with UE5 (project targets 5.8, matching the installed 5.8.3) and Visual Studio 2022. Path: connect the PC through Remote Control so a session can run `Scripts\*.bat` there, or run them manually and paste output.
2. **Persistence of this work** - needs a GitHub repo to push to.

## Supported alternative workflow (in use)
Simulation rules live in the engine-independent `RiskboundSim` module and are built and unit tested here with CMake. The same sources compile as a UE module. UE-side code is written only when it can be compiled and run on the Windows machine.

## Re-audit on Windows
Run the `unreal-project-audit` skill on the Windows machine; it records engine version, compiler, MCP and packaging readiness here.

## Windows PC audit (2026-10-20, via Remote Control)
| Check | Result |
|---|---|
| OS | Windows 11 Home, 10.0.26300 |
| Unreal Engine | 5.8.3 at `C:\Program Files\Epic Games\UE_5.8` (CL 58210709). Stale registry entry for "4.0" (folder missing) - harmless. |
| Compiler | VS Build Tools 2022 17.14 with VC x86/x64 tools (no IDE). Exact MSVC toolset vs UE 5.8 requirement still to confirm at first build. |
| Windows SDK | 10.0.19041.0, 10.0.22621.0 |
| git / gh | git 2.53, gh 2.95 signed in as mutima89 (repo scope) |
| CMake | 3.27.7 |
| Installer tools | None (no Inno Setup, NSIS, WiX). .NET SDK 9.0.200 present. |
| Signing | signtool present in Windows Kits; no certificate known -> releases unsigned |
| Disk | C: 98.9 GB free |
| Working folder | `C:\Downloads\Riskbound` (connected, empty) |

`Riskbound.uproject` EngineAssociation updated to 5.8.
