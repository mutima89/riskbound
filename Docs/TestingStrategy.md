# Testing strategy

| Layer | Tool | Where it runs | Command |
|---|---|---|---|
| Simulation core | `Tests/Core` (dependency-free harness, `TEST`/`CHECK` macros) | Any OS with CMake + C++17 | `Scripts/run_core_tests.sh [filter]` / `Scripts\RunCoreTests.bat` |
| UE integration | UE Automation (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, prefix `Riskbound.`) | Windows + UE5 | `Scripts\RunUnrealTests.bat <UE dir>` |
| Gameplay | Functional tests in `L_Test_M001` (walk, interact, submit, save) | Windows + UE5 | same |
| Packaged | Launch `Riskbound.exe`, play M001, save, quit, relaunch, resume; offline (network disabled) | Windows | manual checklist in `riskbound-qa-release` skill |

## Rules
- Linux core runs use ASan + UBSan by default.
- Every formula has a test with hand-computed expected values.
- Every save-format change gets a migration test from a handcrafted old file.
- Tests must be shown to fail when the rule is broken (spot mutation check per subsystem).
- Record real results (command, date, pass/fail counts) in `Docs/QAReport.md`. Never paste expected output as actual.

## Coverage map (core)
Risk scoring, matrices, control effectiveness, residual, appetite, register CRUD/filter/search, expected loss, cost-benefit, KRIs, sensitivity, stress, Monte Carlo, RNG determinism, career ladder and promotion gates, XP-never-promotes, organisation consequences and mitigation, seeded reproducibility, Mission 001 gating/scoring/consequences, achievements, save round trip, resume determinism, safe write, backup recovery, corrupt-save protection, v1 migration, future-version rejection.
