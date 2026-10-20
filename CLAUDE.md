# Riskbound

Offline, single-player, first-person risk-management career simulator. Unreal Engine 5, C++ + Blueprints, Windows x64.
The player rises from Graduate Trainee to Chief Risk Officer by doing real risk work.

## Architecture (detail: Docs/Architecture.md)
- `Source/RiskboundSim/` - engine-independent C++17 simulation rules (risk, quant, career, organisation, missions, saves). No UObjects, no exceptions, no RTTI. Deterministic.
- `Source/Riskbound/` - UE gameplay/presentation: character, interaction, UMG, UObject adapters calling RiskboundSim.
- `Tests/Core/` - unit tests for RiskboundSim, built by root `CMakeLists.txt` (runs without UE).
- `Data/` - scenario data (to migrate mission content from C++ tables to Data Assets/validated files).

## Commands
- Core tests (Linux/macOS): `Scripts/run_core_tests.sh [filter]`
- Core tests (Windows): `Scripts\RunCoreTests.bat [filter]`
- UE automation tests: `Scripts\RunUnrealTests.bat <UE dir>` (Windows only, unverified until Gate 1)
- Package: `Scripts\BuildWindows.bat <UE dir>` (Windows only, unverified until Gate 6)

## Conventions
- UE naming style (PascalCase, `b` bools) in all C++. Core lives in `namespace rb`, files prefixed `Rb`.
- One formula, one place: every risk/financial formula lives in `RbQuant`/`RbRiskModel` and is documented in Docs/RiskMethodology.md. Never re-implement it in Blueprints or UI.
- Ordinal scores are never money. Expected loss is never presented as worst case.
- Promotion requires competence evidence and passed missions; XP never gates promotion.
- Rubrics stay hidden until the player submits.
- Random events use seeded `rb::Rng` only.
- Bump `CurrentSaveVersion` and add a migration for any save-format change (Docs/SaveFormat.md).

## Rules
- Never claim a build, test or package passed without real output from this session. Record results in Docs/Progress.md.
- No destructive git operations or deleting user saves without explicit approval.
- No network services, accounts, or external AI at runtime.
- Start each task: read Docs/Progress.md, then only the docs and files relevant to the task, then `git diff`.

## Skills (.claude/skills)
`unreal-project-audit`, `riskbound-gameplay`, `riskbound-risk-engine`, `riskbound-career-simulation`, `riskbound-qa-release`.

## Current phase
Gate 1 (Mission 001 vertical slice). Simulation side done and tested; UE gameplay side needs a Windows machine with UE5. See Docs/Progress.md.
