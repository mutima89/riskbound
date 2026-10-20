# QA report

## 2026-10-20 - simulation core (Linux cloud container)
| Run | Command | Result |
|---|---|---|
| GCC 13.3, Debug, ASan+UBSan, `-Wall -Wextra -Wpedantic -Wshadow` | `Scripts/run_core_tests.sh` | 30 cases, 0 failed, 0 warnings, no sanitizer reports |
| Clang 18.1, Release | `cmake --build build-clang20` | 30 cases, 0 failed |
| Clang 18.1, `-std=c++20 -fno-exceptions -fno-rtti` compile of all core sources | per-file compile | clean (UE5 builds C++20) |
| Mutation check: rubric answer key changed L4 -> L3 | `RiskboundCoreTests M001` | 2 cases failed as expected; reverted |

## Not yet tested (blocked: needs Windows + UE5)
UE module compilation, editor/PIE play, automation tests, packaged startup, offline packaged run, installer, UI workflows.
