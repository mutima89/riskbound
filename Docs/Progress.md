# Progress

## Status: Phase 1 of the 30-idea roadmap done (Docs/Roadmap.md); 0.5.1-alpha built locally (not published); whole game walked through the real UI by a scripted driver; hands-on playtest by a person pending
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
- Answer-order fix and Gate 5 (2026-10-21): options shown in a per-save shuffled order; save fuzzing, per-file content validation, deterministic campaign replay with save/load at every step, settings persistence; Shipping rebuilt and its self-tests pass; portable zip built and run.
- Gate 6 (2026-10-21): Inno Setup installer and portable zip for 0.5.0-alpha; silent install, run from the installed folder, uninstall keeping saves verified; release docs (Docs/Release.md, release notes, known issues).
- Shipping Win64 package builds (2026-10-21); Shipping self-test play, resume and tour PASS (Docs/QAReport.md).
- Walkthrough (2026-10-21, Docs/WalkthroughReport.md): `-RbSelfTest=walkthrough`/`walkthrough2` drive the Shipping build through real Slate and Enhanced Input events, new game to the CRO capstone with save, quit and Continue. Fixed: campaign dead end (passed cases reopen as reviews when stuck), longest-answer tell (634 labels rewritten, test-guarded), active tool tab, register clipping and appetite colour, assessment form header and hint, Fullscreen state, top-of-ladder text. 75 core tests, 11 Unreal tests.
- 0.5.1-alpha (2026-10-21): installer and portable zip from the walkthrough commit; install, run, uninstall verified. Not published on GitHub yet (owner stopped the push, 2026-10-21).

- Phase 1 of the 30-idea roadmap (2026-10-21, Docs/Roadmap.md, Docs/GameDesign.md "Living world"): relationships and office politics, inbox with reply deadlines, stress and burnout, regulator events, fallout from weak answers traced to the decision, crisis causes and escalation, The Corvane Ledger story (S001-S004, 10 clues), share index and news, adaptive music and sound (Docs/AssetCredits.md "Audio"), save v6. Verified: 86 core tests, 14 Unreal tests, Shipping self-test play/resume (world_content, inbox, audio checks), Shipping UI walkthrough new game to level 10 with save/quit/Continue (findings: journal headline wrap, binder target; both fixed after the build).
- Phase 2 started: numeric and rating questions (scored by closeness; case files UI, walkthrough and self-test support) and six case studies inspired by real events (M031-M036). Core tests pass; not yet in a packaged build.

### Not yet verified
- A hands-on playthrough by a person (movement feel, mouse sensitivity, readability at all text sizes, volume). The walkthrough sends real UI input but its timing and judgement are not a person's.
- Gamepad (unsupported), resolutions other than 1920x1080, Analyst and Executive difficulty through the UI (covered by core replay tests).

### Next action
Owner plays the phase 1 build (Release\Shipping\Windows\Riskbound.exe). Phase 2: in-world evidence and presenting, tool-scored questions in more missions, living office, own office by level.
Earlier: Owner's hands-on review of 0.5.1-alpha. Open walkthrough findings (Docs/KnownIssues.md): incident frequency for weak choices, crisis briefings naming their cause, binder interaction target, case list grouping, audio.

### Open blockers
- None for building. No full VS IDE installed (Build Tools only); Inno Setup 6.7.3 installed per user (owner-approved, 2026-10-21).

### Gate checklist
- [ ] Gate 1: M001 playable in packaged build, save/exit/relaunch/resume verified (automated checks pass; hands-on playtest outstanding)
- [x] Gate 2: remaining risk tools in UI (Monte Carlo, sensitivity, stress, KRI dashboard, reports) - automated tests and UI captures; hands-on review pending
- [x] Gate 3: career + organisation consequences across missions (sim + UE subsystem, tested; content arrives in Gate 4)
- [x] Gate 4: campaign content (30 missions, 12 crises, CRO capstone), data-driven missions, 50 achievements (verified by tests and packaged self-tests; hands-on playtest pending)
- [x] Gate 5: automated + integration tests (75 core, 11 Unreal, packaged self-tests on Development and Shipping, UI walkthrough on Shipping)
- [x] Gate 6: Windows package, installer, release verification (unsigned; hands-on wizard check and all-users install pending)
