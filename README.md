<div align="center">

# RISKBOUND

**From graduate to Chief Risk Officer, one real decision at a time.**

A first-person career simulator about risk management. Single-player and fully offline, built in Unreal Engine 5 for Windows.

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0b6e79)
![Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-1f2933)
![Version](https://img.shields.io/badge/version-0.5.1--alpha-e0a526)
![Tests](https://img.shields.io/badge/tests-73%20core%20%2B%2011%20Unreal-2f855a)
![License](https://img.shields.io/badge/license-MIT-4a5568)

![Valtren HQ reception](Docs/Screenshots/pass3/after/Tour_Reception.jpg)

</div>

---

## The pitch

You've just joined the risk team at **Valtren Coldline Logistics**, a (fictional) cold-chain company with six sites, a delivery fleet and a board that wants answers. On your first morning, Mission 001 drops on your desk: a temperature alarm at the Northgate cold store has been going off for weeks and nobody acted on it.

Walk the office, talk to your manager, find the evidence, rate the risk, judge the controls, recommend a treatment and defend it. Then live with what happens next, because Riskbound remembers. The contractor you let off the hook in one mission can cause the breakdown that opens a crisis three missions later.

There are no quiz-show answers. You use the tools practitioners use: a risk register and heatmap, KRIs, expected loss, cost-benefit with NPV, sensitivity tornadoes, stress tests and Monte Carlo simulation. You're judged on the quality of your reasoning, not on luck.

## Features

| | |
|---|---|
| 🏢 **A real place to work** | Valtren HQ, explored in first person: reception, analyst floor, manager's office, meeting room, training area, risk monitoring room and boardroom. Lumen lighting, about 70 fps on average at 1080p High on a GTX 1650. |
| 📁 **A full campaign** | 30 missions, 12 crises and a Chief Risk Officer capstone at your first board meeting. Topics include third-party risk, KRIs, insurance, control testing, cyber, appetite, stress testing, audit, acquisitions, liquidity and regulators. There are also four optional specialisms: environmental, project, business continuity and fraud. |
| 🧮 **Nine working risk tools** | Risk register with heatmap, KRI dashboard, loss scenarios, cost-benefit (NPV/ROI), sensitivity tornado, stress test, Monte Carlo, action tracker and an executive report you can export. Every formula lives in one place and is documented. |
| 🔗 **Consequences that last** | Decisions move organisation metrics, set flags later briefings mention, and schedule incidents that may or may not happen. Incidents are seeded and reproducible. Good controls make later incidents less likely or less severe, and some incidents open crises. |
| 🎓 **Promotion you earn** | Ten career levels and ten competencies. You're promoted on competence evidence and passed missions only; XP never buys a promotion. Your approval authority grows with your rank, and spending above it without escalating costs you. |
| 🏆 **50 achievements** | They're tracked in your save, plus a journal that tells you exactly what your next promotion still needs. |
| ♿ **Accessible** | Text size, reduced motion, hints, volume, mouse sensitivity, graphics presets from Low to Epic, and fully keyboard-navigable menus. |
| 💾 **Saves you can trust** | Automatic checkpoints, versioned and checksummed, with a backup that loads if the main save is damaged. Local only, with no accounts and no network. |

## Screenshots

| | |
|---|---|
| ![Boardroom](Docs/Screenshots/pass3/after/Tour_Executive_boardroom.jpg) | ![Risk monitoring room](Docs/Screenshots/pass3/after/Tour_Risk_monitoring_room__operators_.jpg) |
| *The executive boardroom* | *The risk monitoring room* |
| ![Mission 001 assessment](Docs/Screenshots/ui/Ui_06_Assessment.jpg) | ![Sensitivity tornado](Docs/Screenshots/ui/Ui_15_Tools_Sensitivity.jpg) |
| *Mission 001: the full risk assessment* | *Sensitivity tornado on a treatment's NPV* |
| ![Case file](Docs/Screenshots/ui/Ui_22_CaseMission.jpg) | ![Debrief](Docs/Screenshots/ui/Ui_23_CaseDebrief.jpg) |
| *A case file: briefing, evidence and questions* | *The debrief, revealed only after you submit* |

## The career ladder

| Level | Role | Missions to pass before the next promotion |
|---:|---|---|
| 2 | Junior Risk Analyst *(you start here)* | M001 to M003 |
| 3 | Risk Analyst | M004 to M006 |
| 4 | Senior Risk Analyst | M007 to M009 |
| 5 | Supervisor / Team Lead | M010 to M012 |
| 6 | Risk Manager | M013 to M015 |
| 7 | Senior Risk Manager | M016 to M018 and crisis C001 |
| 8 | Head of Risk | M019 to M022 and crisis C002 |
| 9 | Director of Risk | M023 to M026 and crisis C003 |
| 10 | **Chief Risk Officer** | Your first board meeting: the capstone |

Passing missions isn't enough on its own. Each promotion also needs minimum competency scores; for example, you need Communication 45 or better to move up from Junior Risk Analyst. Full model: [Docs/CareerModel.md](Docs/CareerModel.md).

## Install and play

1. Get `Riskbound-Setup-0.5.1-alpha.exe` (installer, no admin rights needed) or `Riskbound-0.5.1-alpha-Win64-Portable.zip` (unzip and run `Riskbound.exe`). Check it against the `.sha256` file next to each download.
2. **Windows SmartScreen will warn "Unknown publisher"**, because the build isn't code-signed. Choose **More info, then Run anyway**. See `UNSIGNED-BUILD.txt` for details.
3. Uninstalling keeps your saves in `%LOCALAPPDATA%\Riskbound`.

**Requirements:** Windows 10/11 64-bit with a DirectX 12 GPU. Tested on a GeForce GTX 1650 at 1920x1080 on High. Use Low or Medium on weaker hardware.

**Controls:**
- WASD or arrow keys move, and the mouse looks around.
- **E** interacts, **J** opens the journal and **Esc** pauses.
- In menus, use Tab or the arrow keys to move, Enter or Space to select, and Esc to go back.

## Build from source

The game is split so the important logic can be tested without Unreal:

```
Source/RiskboundSim/   engine-independent C++17 rules: risk model, quant, career, missions, saves (deterministic, no RTTI or exceptions)
Source/Riskbound/      Unreal gameplay and UI: character, interaction, Slate screens, adapters into the sim
Data/Missions/         the campaign as validated JSON (format: Docs/MissionData.md)
Tests/Core/            simulation unit tests (CMake, run anywhere)
Docs/                  design, risk methodology, career model, save format, QA, release
```

**Simulation tests** (Linux, macOS or Windows; just CMake and a C++17 compiler):

```bash
Scripts/run_core_tests.sh          # Linux/macOS
Scripts\RunCoreTests.bat           # Windows
```

**The game** (Windows, Unreal Engine 5.8 and Visual Studio 2022 with the C++ workload):

```bat
set UE="C:\Program Files\Epic Games\UE_5.8"
%UE%\Engine\Build\BatchFiles\Build.bat RiskboundEditor Win64 Development -project="%CD%\Riskbound.uproject"
Scripts\RunUnrealTests.bat %UE%                     :: automation tests
Scripts\BuildWindows.bat %UE% Shipping              :: -> Release\Shipping\Windows\Riskbound.exe
Scripts\BuildInstaller.bat 0.5.1-alpha 0.5.1.0      :: installer + zip (needs Inno Setup 6)
```

Packaged self-tests: `Riskbound.exe -RbSelfTest=play`, then `-RbSelfTest=resume`. Release checklist: [Docs/Release.md](Docs/Release.md).

## How it's tested

- **73 simulation tests.** One plays the best answer to every question from Mission 001 to the capstone and checks promotion to CRO on competence alone. Others replay the whole campaign with random answers to prove it's deterministic, throw 4,000 damaged save files at the loader, and check every figure quoted in mission answers against the game's own formulas.
- **11 Unreal automation tests**, plus packaged self-tests (play, resume and a room tour with fps) on the Shipping build.
- Every mission file is validated at load. Broken content fails the tests, so it can't ship.

Details: [Docs/TestingStrategy.md](Docs/TestingStrategy.md) and [Docs/QAReport.md](Docs/QAReport.md). The maths is in [Docs/RiskMethodology.md](Docs/RiskMethodology.md).

## Honest status

This is an **alpha**. Everything above is built and passes automated tests, but:

- The campaign after Mission 001 hasn't been played through by a person yet, so pacing and difficulty still need tuning.
- There's no music or sound yet.
- Missions after 001 are played on the Case files screen rather than as walk-around scenes.
- Your manager still uses the Unreal mannequin as a stand-in body.
- Keyboard and mouse only; no gamepad.
- English only.

Full list: [Docs/KnownIssues.md](Docs/KnownIssues.md). Progress log: [Docs/Progress.md](Docs/Progress.md).

## A note on names

Every company, person and brand in Riskbound is fictional: Valtren Coldline Logistics, Kelvarin Refrigeration Services and the rest. Any resemblance to real organisations is coincidental.

## Credits

**Created by M A Almahlaki**
📧 m.awwadh.k@gmail.com · 💼 [LinkedIn](https://www.linkedin.com/in/mohammed-almahlaki001/)

- Built with Unreal Engine 5.8. Unreal is a trademark of Epic Games, Inc.
- Third-party assets (CC0) and the Inter font (SIL OFL) are credited to their authors in [Docs/AssetCredits.md](Docs/AssetCredits.md).
- Development assistance: Claude Code by Anthropic.
- The same credits appear in the game (main menu and pause menu, then Credits). Look closely around the office, too.

## License

Code is MIT; see [LICENSE](LICENSE). Third-party assets keep their own licences, listed in [Docs/AssetCredits.md](Docs/AssetCredits.md).
