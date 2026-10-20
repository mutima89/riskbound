# Progress

## Status: Gate 1 implemented and verified by tests; hands-on playtest pending
Last update: 2026-10-20

### Done
- Gate 0 audit (Docs/EnvironmentAudit.md).
- Repo scaffold, CLAUDE.md, five project skills, canonical docs.
- `RiskboundSim` core: risk register + matrix + controls + appetite; quant tools (EL, NPV/ROI, KRI, sensitivity, stress, Monte Carlo); career ladder + promotion gates; organisation state + deferred consequences; Mission 001 content, hidden rubric, debrief; achievements (7); versioned saves with backup recovery + v1 migration. 30 core unit tests (Docs/QAReport.md).
- Windows PC verification (2026-10-20, UE 5.8.3, VS 2022 Build Tools 17.14, MSVC 14.44.35229, SDK 10.0.22621.0).
- Gate 1 UE layer (Docs/Architecture.md "Riskbound UE module"): game + save subsystems, first-person character with code-built Enhanced Input, interaction component and interactables, runtime-built seven-room HQ graybox in `L_MeridianHQ`, Slate UI (HUD, main menu with difficulty and Continue, pause, settings, journal, workstation, document viewer, dialogue, assessment form, debrief), autosave checkpoints, backup-recovery notice, `-RbSelfTest`, 3 automation tests.
- Sim headers export their API (`RB_API`) so the editor DLL build links; no behaviour change.
- Development Win64 package builds; packaged self-test play + resume PASS; packaged game starts to the main menu (Docs/Screenshots/01_main_menu.png).

### Not yet verified
- A full hands-on playthrough by a person (movement feel, every screen with mouse and keyboard, text readability at all text sizes, fullscreen toggle, volume). The self-test drives the same HUD/interactable calls but not real key presses.
- Shipping configuration package.
- Save/exit/relaunch/resume by hand (verified only through the self-test process pair and automation tests).

### Next action
Hands-on playtest of Mission 001 in the packaged build; fix findings. Then close Gate 1 and start Gate 2 (Monte Carlo, sensitivity, stress, KRI dashboard, reports in UI).

### Open blockers
- None for building. No full VS IDE installed (Build Tools only); Inno Setup/NSIS/WiX not installed (needed by Gate 6).

### Gate checklist
- [ ] Gate 1: M001 playable in packaged build, save/exit/relaunch/resume verified (automated checks pass; hands-on playtest outstanding)
- [ ] Gate 2: remaining risk tools in UI (Monte Carlo, sensitivity, stress, KRI dashboard, reports)
- [ ] Gate 3: career + organisation consequences across missions
- [ ] Gate 4: campaign content (30 missions, 12 crises, CRO capstone), data-driven missions
- [ ] Gate 5: automated + integration tests
- [ ] Gate 6: Windows package, installer, release verification
