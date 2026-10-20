---
name: riskbound-gameplay
description: Implement or change Riskbound first-person gameplay in the Unreal Riskbound module (character, Enhanced Input, interaction, NPCs, workstations, mission objectives wired to the simulation). Use for UE-side gameplay work, not for risk formulas or career rules.
---

# Riskbound gameplay

Read first: `Docs/Architecture.md` (UE module section), `Docs/GameDesign.md` (mission tables). Read the RiskboundSim header you are wiring to; do not read the whole core.

## Rules
- Gameplay never computes risk, scores or consequences. It calls `rb::` APIs through `URbGameSubsystem` and displays the results.
- Interactables carry a string id; the mission director maps ids to sim calls (e.g. evidence id -> `MissionState::ViewEvidence`). No mission logic in level Blueprints beyond triggering those calls.
- Enhanced Input: one Input Mapping Context, actions Move, Look, Interact, Pause, Journal. Mouse sensitivity and reduced motion come from `rb::PlayerSettings`.
- Every interactable shows a contextual prompt and gives feedback on use. Every UI screen must be keyboard navigable.
- Never reveal the M001 rubric before `Submit()`; the debrief reads `MissionState::Result`.
- Autosave via `URbSaveSubsystem` at checkpoints (after meeting manager, after submit).

## Verify
1. Compile `RiskboundEditor Win64 Development`.
2. Add or update an automation/functional test under `Riskbound.Gameplay.*` for the behaviour.
3. Run `Scripts\RunUnrealTests.bat`. Paste the summary line into `Docs/QAReport.md`.
4. For anything visual, play in editor and state what you observed; if you could not run it, say so.
