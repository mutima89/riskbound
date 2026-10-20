---
name: riskbound-career-simulation
description: Implement or change Riskbound career levels, competencies, promotion gates, organisational state, decision consequences, missions' prerequisites and their persistence. Use for RbCareer, RbOrganization, mission scoring/consequence code and the save fields they need.
---

# Riskbound career and organisation simulation

Canonical rules: `Docs/CareerModel.md`, `Docs/GameDesign.md` (Consequences), `Docs/SaveFormat.md`. Code: `RbCareer.*`, `RbOrganization.*`, `RbMission*.cpp`, `RbGameState.*`, `RbSave.cpp`.

## Rules
- Promotion = competency thresholds + evidence counts + passed missions. XP must never be an input; keep `Career_XpAloneNeverPromotes` passing.
- Missions produce `AssessmentResult`s from a rubric that is hidden until submission. Rubric scores decision quality; luck lives only in `OrganizationState::AdvanceWeeks`.
- Consequences are `PendingConsequence`s with a trigger week, probability, effects and an optional mitigating flag. No ad-hoc consequence code in gameplay.
- Mission prerequisites are checked in the sim (`Validate()`-style), not only in UI.
- New persistent field: add to `GameState`, write + read in `RbSave.cpp`, bump `CurrentSaveVersion` with a migration if old saves lack it, extend `Save_RoundTripIsExact`.

## Done means
- Unit tests for the gate/consequence (including the negative case) and a save round trip covering new fields.
- `Scripts/run_core_tests.sh` green; Docs updated; one line in `Docs/Progress.md`.
