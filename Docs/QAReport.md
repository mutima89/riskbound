# QA report

## 2026-10-20 - simulation core (Linux cloud container)
| Run | Command | Result |
|---|---|---|
| GCC 13.3, Debug, ASan+UBSan, `-Wall -Wextra -Wpedantic -Wshadow` | `Scripts/run_core_tests.sh` | 30 cases, 0 failed, 0 warnings, no sanitizer reports |
| Clang 18.1, Release | `cmake --build build-clang20` | 30 cases, 0 failed |
| Clang 18.1, `-std=c++20 -fno-exceptions -fno-rtti` compile of all core sources | per-file compile | clean (UE5 builds C++20) |
| Mutation check: rubric answer key changed L4 -> L3 | `RiskboundCoreTests M001` | 2 cases failed as expected; reverted |

## 2026-10-20 - Gate 1 on Windows (UE 5.8.3, MSVC 14.44.35229, SDK 10.0.22621.0)
| Run | Command | Result |
|---|---|---|
| Core tests (MSVC /W4) after `RB_API` change | `Scripts\RunCoreTests.bat` | `30 test cases, 0 failed, 0 failed checks`, no warnings |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded`, 0 warnings, 0 errors |
| Automation tests | `Scripts\RunUnrealTests.bat` | 3/3 `Result={Success}`: Riskbound.Gameplay.SubmissionNeedsBriefingAndEvidence, Riskbound.Save.RecoversFromCorruptPrimary, Riskbound.Session.PlaySaveLoadRoundTrip; `TEST COMPLETE. EXIT CODE: 0` |
| Package Development Win64 | `Scripts\BuildWindows.bat <UE> Development` | `BUILD SUCCESSFUL`, `AutomationTool exiting with ExitCode=0`, 504 packages cooked, 0 warnings in UAT output |
| Packaged self-test | `Riskbound.exe -RbSelfTest=play` | `PASS`: 5 evidence props + NPC + workstation found, 7/7 evidence viewed, submit ok, score 100 passed, checkpoint `M001:debriefed`, saved |
| Packaged self-test | `Riskbound.exe -RbSelfTest=resume` | `PASS` (Saved/RbSelfTest.txt): load outcome Ok, objective "Mission 001 complete...", career level 2 Junior Risk Analyst, passed missions M001, R-014 treatment Reduce (owner ops_site_manager, In treatment) |
| Normal launch | `Riskbound.exe -windowed` | Starts to the main menu in reception, Continue disabled with "No saved game yet." (Docs/Screenshots/01_main_menu.png); log shows HQ built (71 components, 7 rooms), no errors |

What the automated checks cover: every mission action goes through `URbGameSubsystem` (the same calls the UI buttons make), save/load equality via `rb::SerializeGame`, backup recovery and both-corrupt handling, submission gating and hidden rubric before submit.

## Not yet tested
Hands-on play with real input (movement, look sensitivity, each screen by mouse and keyboard, text size, fullscreen toggle, volume), Shipping package, installer, offline packaged run on a second machine.

## 2026-10-20 - Visual quality pass, step 1 (HQ art)
| Run | Command | Result |
|---|---|---|
| Asset import | `Scripts\ImportAssets.bat` | `done: 13 meshes, 6 materials, 0 errors`; Content/Riskbound 43 MB |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded`, 0 warnings |
| Automation tests | `Scripts\RunUnrealTests.bat` | 3/3 `Result={Success}` |
| Tour, before (graybox) | `UnrealEditor.exe Riskbound.uproject -game -RbSelfTest=tour -ResX=1920 -ResY=1080` | average 67.5 fps (14.8 ms) |
| Tour, after (art) | same | rooms 52.0-60.3 fps (16.6-19.2 ms, GPU 15.7-19.1 ms); the first stop (corridor) includes the editor's first-use shader compile and is not representative |

Performance is measured in the editor's -game mode (uncooked), 1920x1080, on this PC's 4 GB GPU; a cooked build is expected to be faster. After screenshots: Docs/Screenshots/after/. The graybox "before" screenshots were not kept (overwritten by later tours); Docs/Screenshots/01_main_menu.png shows the graybox reception.

## 2026-10-20 - Visual quality pass, step 2 (UI restyle, graphics quality)
| Run | Command | Result |
|---|---|---|
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded`, 0 warnings |
| Automation tests | `Scripts\RunUnrealTests.bat` | 3/3 `Result={Success}` |
| UI capture | `UnrealEditor.exe Riskbound.uproject -game -RbSelfTest=ui -ResX=1920 -ResY=1080` | 11 screens captured through the HUD calls (Docs/Screenshots/ui/); Inter loaded, focus outline visible on the focused control |

## 2026-10-20 - Visual polish round 2 (density, lighting, UI fixes, performance)
| Run | Command | Result |
|---|---|---|
| Core tests | `Scripts\RunCoreTests.bat` | `46 test cases, 0 failed, 0 failed checks` (adds FormatMoney and debrief money-format tests) |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded`, 0 warnings |
| Automation tests | `Scripts\RunUnrealTests.bat` | 3/3 `Result={Success}` |
| Asset import | `Scripts\ImportAssets.bat` | 13 meshes, 7 master materials, 0 errors |
| Package Development | `Scripts\BuildWindows.bat <UE> Development` | `BUILD SUCCESSFUL` |
| Packaged self-test | `-RbSelfTest=play`, `-RbSelfTest=resume` | both PASS |
| Packaged tour x3, 1920x1080, default quality (High: 70% internal resolution, TSR) | `Riskbound.exe -RbSelfTest=tour -ResX=1920 -ResY=1080` | every room 62.9-75.7 fps (GPU 12.5-16.4 ms) in all three runs; first stop (corridor) 42.8-59.1 includes start-up warm-up; 0 material usage warnings in the game log |

Defect found and fixed during this round: after switching repeated geometry to instanced static meshes, the cooked game replaced every material lacking the "used with instanced static meshes" flag by the default material (the editor hid it by compiling usages on demand). Earlier packaged FPS figures from that interval were measured on placeholder materials and are discarded. The import script now sets the flag on every master material, and the packaged log is checked for `missing usage flag`.

Graphics quality presets: Low 50%, Medium 60%, High 70%, Epic 100% internal resolution (TSR upscale); Low also turns Lumen off via engine scalability.
