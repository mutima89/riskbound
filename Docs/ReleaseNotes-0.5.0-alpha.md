# Riskbound 0.5.0-alpha - release notes

First complete build: the whole career from Junior Risk Analyst to Chief Risk Officer is playable. Offline, single-player, Windows 10/11 64-bit.

## What's in it
- **Valtren HQ**, a first-person office (reception, analyst workspace, manager's office, meeting room, training area, risk monitoring room, boardroom) built from free CC0 assets, with lighting and materials tuned for 60+ fps at 1080p on a GTX 1650.
- **Mission 001, "The Unresolved Warning"**: investigate a cold-store temperature risk in the building - talk to your manager, find seven pieces of evidence, complete a full assessment (missing information, inherent and residual risk, control effectiveness, treatment, action owner, rationale) and get a debrief.
- **Campaign**: 29 more missions, 12 crises and a CRO capstone at the workstation's Case files: third-party risk, KRIs, quantification and insurance, control testing, near misses, Monte Carlo, challenging first-line self-assessments, team leadership, appetite breaches, cyber, risk appetite, stress testing, audit findings, acquisitions, supplier distress, programmes, enterprise risk, board reporting, emerging risks, culture, board committees, liquidity, regulators, strategy under uncertainty. Four optional specialisms: environmental, project, business continuity, fraud.
- **Consequences that last**: decisions change the organisation's metrics, set flags later missions refer to, and schedule incidents that may or may not happen (seeded, reproducible). Good controls you put in place make later incidents less likely or less damaging; some incidents open crises.
- **Career**: ten levels, ten competencies. Promotion needs competence evidence and passed missions; XP never promotes you. Decision authority grows with your level - spending above it without escalating costs you.
- **Nine risk tools** at the workstation: register, KRI dashboard, loss scenarios, cost-benefit, sensitivity (tornado), stress test, Monte Carlo, action tracker, executive report export.
- **50 achievements**, journal with promotion gaps, credits screen.
- **Settings**: text size, reduced motion, hints, volume, mouse sensitivity, graphics quality (Low to Epic), fullscreen.
- **Saves**: automatic checkpoints, versioned and checksummed, with a backup that loads if the main save is damaged.

## Quality
73 simulation tests and 11 Unreal automation tests; packaged self-tests (play, resume, tour) pass on Shipping; the installer was installed, run and uninstalled in testing. A test plays the best answers through the entire campaign and checks promotion to CRO. See Docs/QAReport.md.

## Install
Run `Riskbound-Setup-0.5.0-alpha.exe` (no administrator rights needed), or unzip `Riskbound-0.5.0-alpha-Win64-Portable.zip` and run `Riskbound.exe`. The build is unsigned: see `UNSIGNED-BUILD.txt`. Uninstalling keeps your saves in `%LOCALAPPDATA%\Riskbound`.

## Known issues
See Docs/KnownIssues.md.
