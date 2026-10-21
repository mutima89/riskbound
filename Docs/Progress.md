# Progress

## Status: Gate 1 verified by tests (hands-on playtest pending); Gate 2 tools in the UI; Shipping package verified
Last update: 2026-10-21

### Done
- Gate 0 audit (Docs/EnvironmentAudit.md).
- Repo scaffold, CLAUDE.md, five project skills, canonical docs.
- `RiskboundSim` core: risk register + matrix + controls + appetite; quant tools (EL, NPV/ROI, KRI, sensitivity, stress, Monte Carlo); career ladder + promotion gates; organisation state + deferred consequences; Mission 001 content, hidden rubric, debrief; achievements (7); versioned saves with backup recovery + v1 migration. 30 core unit tests (Docs/QAReport.md).
- Windows PC verification (2026-10-20, UE 5.8.3, VS 2022 Build Tools 17.14, MSVC 14.44.35229, SDK 10.0.22621.0).
- Gate 1 UE layer (Docs/Architecture.md "Riskbound UE module"): game + save subsystems, first-person character with code-built Enhanced Input, interaction component and interactables, runtime-built seven-room HQ graybox in `L_ValtrenHQ`, Slate UI (HUD, main menu with difficulty and Continue, pause, settings, journal, workstation, document viewer, dialogue, assessment form, debrief), autosave checkpoints, backup-recovery notice, `-RbSelfTest`, 3 automation tests.
- Sim headers export their API (`RB_API`) so the editor DLL build links; no behaviour change.
- Development Win64 package builds; packaged self-test play + resume PASS; packaged game starts to the main menu (Docs/Screenshots/01_main_menu.png).
- Fictional business names everywhere (save v4 migrates old names); CC0/OFL asset pipeline (Docs/AssetCredits.md); UI restyle and visual polish round 2 (60+ fps packaged at 1080p High); credits screen and README credits; three easter eggs.
- Gate 2 workstation tool hub: risk register (add/edit/filter/search), 5x5 matrix, KRI dashboard with history, loss scenarios, cost-benefit, sensitivity tornado, stress presets, seeded Monte Carlo with histogram, action tracker with weeks, executive report export. 46 core tests, 8 Unreal automation tests.
- Visual pass 3 (2026-10-21): lighter, thinner ceiling grid; busier risk monitoring room; clean polished concrete (Docs/Screenshots/pass3).
- Gate 3 (2026-10-21): campaign missions in the sim (`RbCampaign`) and UE (`URbCampaignSubsystem`): availability gates, hidden-rubric scoring with decision authority, competence-based promotion across missions, consequences and flags that carry between missions, save v5. 56 core tests, 9 Unreal tests.
- Gate 4 step 1 (2026-10-21): mission content as validated JSON in `Data/Missions` (Docs/MissionData.md); M002-M005 written; case files screen at the workstation; packaged play/resume include M002.
- Gate 4 scale-up (2026-10-21): 30 missions (M001 + M002-M030, four optional specialisms), 12 crises (3 on the career ladder, 9 triggered by incidents), CRO capstone, 50 data-driven achievements; best play reaches CRO in the core test.
- Shipping Win64 package builds (2026-10-21); Shipping self-test play, resume and tour PASS (Docs/QAReport.md).

### Not yet verified
- A full hands-on playthrough by a person (movement feel, every screen with mouse and keyboard, text readability at all text sizes, fullscreen toggle, volume). The self-test drives the same HUD/interactable calls but not real key presses.
- Save/exit/relaunch/resume by hand (verified only through the self-test process pair and automation tests).

### Next action
Owner's hands-on review of the build; fix findings. Owner's hands-on review of the campaign; content tuning from playtests (difficulty of distractors, pacing of weeks). Then Gate 5 (broader automated and integration tests) and Gate 6 (installer, release verification).

### Open blockers
- None for building. No full VS IDE installed (Build Tools only); Inno Setup/NSIS/WiX not installed (needed by Gate 6).

### Gate checklist
- [ ] Gate 1: M001 playable in packaged build, save/exit/relaunch/resume verified (automated checks pass; hands-on playtest outstanding)
- [x] Gate 2: remaining risk tools in UI (Monte Carlo, sensitivity, stress, KRI dashboard, reports) - automated tests and UI captures; hands-on review pending
- [x] Gate 3: career + organisation consequences across missions (sim + UE subsystem, tested; content arrives in Gate 4)
- [x] Gate 4: campaign content (30 missions, 12 crises, CRO capstone), data-driven missions, 50 achievements (verified by tests and packaged self-tests; hands-on playtest pending)
- [ ] Gate 5: automated + integration tests
- [ ] Gate 6: Windows package, installer, release verification
