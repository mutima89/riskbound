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

## 2026-10-20 - Credits (step 3)
| Run | Command | Result |
|---|---|---|
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded` |
| Automation tests | `Scripts\RunUnrealTests.bat` | 4/4 `Result={Success}`, incl. Riskbound.Credits.IncludeCreatorContactAndAttribution (exact email and LinkedIn URL, Epic attribution, asset list, Claude Code) |
| UI capture | `-game -RbSelfTest=ui` | Credits screen captured (Docs/Screenshots/ui/Ui_10_Credits.jpg) |

## 2026-10-21 - Gate 2 workstation tool hub
| Run | Command | Result |
|---|---|---|
| Core tests | `Scripts\RunCoreTests.bat` | `46 test cases, 0 failed, 0 failed checks` |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded` |
| Automation tests | `Scripts\RunUnrealTests.bat` | 8/8 `Result={Success}`: adds Riskbound.Tools.RegisterAddFilterSearchMatrix, .QuantViewModels (EL 117,500 / worst 1,800,000, tornado swing 60,561.78, stressed EL 567,500, seeded Monte Carlo, histogram counts every trial), .KrisActionsAndTime, .ReportExport (file equals on-screen report) |
| UI capture | `-game -RbSelfTest=ui` | all 9 tool tabs captured (Docs/Screenshots/ui/Ui_11..19_Tools_*.jpg) |

## 2026-10-21 - Shipping package check
| Run | Command | Result |
|---|---|---|
| Package Shipping (first attempt) | `Scripts\BuildWindows.bat <UE> Shipping` | FAILED: `SRbToolHub.cpp(24): error C2084: function 'Click' already has a body`. The Shipping unity build merges SRbToolHub.cpp and RbScreens.cpp, so their anonymous-namespace helpers collided. Fixed by prefixing the hub helpers (`HubClick`, `HubNum`, `HubPct`, `HubCard`, `HubCell`, `HubKeyValue`, `HubRatingColor`, `HubStatusColor`); no other same-signature helpers exist across UI/subsystem files. |
| Package Shipping | `Scripts\BuildWindows.bat <UE> Shipping` | `BUILD SUCCESSFUL`, 0 compiler warnings -> `Release\Shipping\Windows\Riskbound.exe` |
| Shipping self-test play | `Riskbound.exe -RbSelfTest=play` | PASS (12 checks; result in `%LOCALAPPDATA%\Riskbound\Saved\RbSelfTest_play.txt`; Shipping writes Saved under %LOCALAPPDATA%, not next to the exe) |
| Shipping self-test resume | `Riskbound.exe -RbSelfTest=resume` | PASS (career level 2, M001 passed, R-014 treatment restored) |
| Shipping tour, 1920x1080, default High | `Riskbound.exe -RbSelfTest=tour -ResX=1920 -ResY=1080` | PASS, average 71.7 fps; rooms 62.1 (corridor, includes warm-up) to 81.3 fps, GPU 11.7-15.3 ms |
| Editor compile after the rename | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded` |
| Automation tests | `Scripts\RunUnrealTests.bat` | 8/8 `Result={Success}` |

## 2026-10-21 - Visual pass 3 (ceiling grid, monitoring room, polished concrete)
| Run | Command | Result |
|---|---|---|
| Textures | `python Scripts\GenerateTextures.py` | adds `T_PolishedConcrete` (own, tileable, 2.4 m slabs) and `T_ScreenKriTicker` (invented KRIs, not Mission 001 figures) |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded` |
| Asset import | `Scripts\ImportAssets.bat` | `done: 13 meshes, 7 materials, 0 errors`; new `MI_FloorPolished`, `MI_CeilingGrid`, `MI_ScreenKriTicker` |
| Package Development | `Scripts\BuildWindows.bat <UE> Development` | `BUILD SUCCESSFUL` |
| Packaged tour, 1920x1080, default High | `Riskbound.exe -RbSelfTest=tour -ResX=1920 -ResY=1080` | PASS, average 74.3 fps; rooms 66.5 (corridor, includes warm-up) to 81.4 fps; new reverse view "Risk monitoring room (operators)" 75.2 fps; 0 `missing usage flag` lines in the game log |

Changes: ceiling T-bars 0.8 cm (was 1.5 cm) in a self-lit light grey instead of unlit paper that read black; monitoring room gets a second operator row facing the video wall, a KRI status rail under the wall, a wall clock, five acoustic panels, two plants; monitoring room and corridor floors use clean polished concrete (the old scanned texture was brown and stained). Screen textures are now never-streamed: the thin ticker otherwise rendered from a 4-pixel-high mip.

Defect found and fixed: the KRI ticker first rendered as an unreadable dark strip because texture streaming kept a tiny mip for a 35 cm-high screen. Fixed by `never_stream` on generated screen textures (Scripts/ImportAssets.py).

Screenshots: before `Docs/Screenshots/pass3/before/`, after `Docs/Screenshots/pass3/after/`.

## 2026-10-21 - Gate 3: career and organisation across missions
| Run | Command | Result |
|---|---|---|
| Core tests | `Scripts\RunCoreTests.bat` | `56 test cases, 0 failed, 0 failed checks`; core library builds at /W4 with no warnings. New: Campaign_ValidatorAcceptsFixtureAndRejectsBrokenContent, Campaign_AvailabilityIsCheckedInTheSim, Campaign_SubmitNeedsEvidenceAndEveryAnswer, Campaign_ScoringSingleMultiAndAuthority (multi 25, authority cap 40, overall 56), Campaign_PromotionNeedsCompetenceAndMissionsAcrossMissions, Campaign_PassingMissionsWithWeakCompetenceDoesNotPromote (communication 80 -> 56 -> 42 < 45), Campaign_ConsequencesCarryIntoLaterMissions (M002 deferral -> fire during M003 -> crisis opens; mitigated variant never opens it), Campaign_FailedMissionCanBeRetriedAndRecordsEachAttempt, Save_V5RoundTripsCampaignAndConsequenceFlags, Save_MigratesV4WithoutCampaign |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded` |
| Automation tests | `Scripts\RunUnrealTests.bat` | 9/9 `Result={Success}`, incl. Riskbound.Campaign.ProgressPromotionAndHiddenRubric (closed before M001, invalid catalogue rejected, evidence gate, debrief hidden until submit, promotion to Risk Analyst after M003) |

Save format is now v5 (Docs/SaveFormat.md). The shipped game has no campaign content yet: Gate 4 adds data files and the mission screen.

## 2026-10-21 - Gate 4 step 1: data-driven missions (M002-M005), validator, case files screen
| Run | Command | Result |
|---|---|---|
| Core tests | `Scripts\RunCoreTests.bat` | `61 test cases, 0 failed, 0 failed checks`, no warnings. New: Json_ParsesValuesEscapesAndReportsErrorsWithLines, MissionData_StrictKeysTypesAndNames (typo key, wrong type, unknown competency/metric, file-name/id mismatch, malformed JSON all reported with file and line), Data_ShippedMissionsParseAndValidate, Data_BestPlayPromotesThroughTheShippedMissions (M001 -> M002 -> M003 promotes to level 3; M004/M005 open; level 4 blocked on M006), Data_M004FiguresMatchTheQuantEngine (EL 20,800 / 10,800 / 3,300, NPV about -27,000 and -1,100) |
| Editor compile | `Build.bat RiskboundEditor Win64 Development` | `Result: Succeeded` |
| Automation tests | `Scripts\RunUnrealTests.bat` | 10/10 `Result={Success}`, incl. Riskbound.Campaign.ShippedMissionDataLoads and the checkpoint regression below |
| Package Development | `Scripts\BuildWindows.bat <UE> Development` | `BUILD SUCCESSFUL`; log: `Mission data: 4 missions from ../../../Riskbound/Data/Missions (0 problems)` |
| Packaged self-test play | `-RbSelfTest=play` | PASS: M001 as before, then M002 through the case files screen: submitted, 4 missions / 0 problems, M002 passed (93), M003 open, objective "Open Case files...", checkpoint M002:debriefed |
| Packaged self-test resume | `-RbSelfTest=resume` | PASS: same checks after reload (M001, M002 passed; campaign record restored) |
| UI capture | `-RbSelfTest=ui` | 24 screens incl. Docs/Screenshots/ui/Ui_21_CaseFiles.jpg, Ui_22_CaseMission.jpg, Ui_23_CaseDebrief.jpg |

Defect found by the packaged self-test and fixed: once a campaign mission moved the save checkpoint (`M002:start`), Mission 001 no longer counted as debriefed, so the objective reverted to "Receive your debrief from Priya Okafor". `IsDebriefed` now also holds when M001 is submitted and the checkpoint has moved past M001; covered by assertions in Riskbound.Campaign.ProgressPromotionAndHiddenRubric.
