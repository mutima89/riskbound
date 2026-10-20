# Architecture

## Layers
```
UMG widgets / Blueprints (presentation, level scripting)
        |  read view-models, send player intents
Riskbound module (UE): ARbCharacter, URbInteractionComponent, ARbInteractable,
        URbGameSubsystem (owns rb::GameState), URbSaveSubsystem, mission director
        |  plain C++ calls, no UObjects below this line
RiskboundSim module (C++17, deterministic, unit tested outside UE)
```

## RiskboundSim (done, tested)
| File | Responsibility |
|---|---|
| `RbRng.h` | Seeded xoshiro256** RNG: uniform, normal, lognormal, Poisson |
| `RbRiskModel` | Matrix, control assessment, residual, appetite, risk register CRUD/filter/search |
| `RbQuant` | Expected loss, ALE, cost-benefit/NPV/ROI, KRIs, sensitivity, stress, Monte Carlo |
| `RbCareer` | Ladder, competencies, promotion gates, pathways |
| `RbOrganization` | Metrics, departments, three profiles, decision records, deferred consequences |
| `RbMission001` | Mission 001 content, objectives, hidden rubric, debrief, consequences |
| `RbGameState` | Persistent state, new game, achievements |
| `RbSave` | Versioned checksummed saves, safe writes, backup recovery, migration |

## Riskbound UE module (Gate 1, not yet written)
Planned classes. Each is thin; anything rule-like calls RiskboundSim.
- `ARbCharacter` - first-person pawn, Enhanced Input (move, look, interact, pause), sensitivity from settings.
- `URbInteractionComponent` - line trace, focus highlight, contextual prompt text.
- `ARbInteractable` (+ `ARbEvidenceProp`, `ARbWorkstation`, `ARbNpc`) - each carries an id the mission director maps to sim calls (`ViewEvidence`, `MeetManager`).
- `URbGameSubsystem` (GameInstanceSubsystem) - owns `rb::GameState`, exposes Blueprint-callable view functions.
- `URbSaveSubsystem` - wraps `rb::SaveGameToFile` with `FPaths::ProjectSavedDir()/SaveGames`, autosave at checkpoints.
- UMG: workstation (risk register, assessment form, NPV calculator), document viewer, dialogue, debrief, pause/settings.
- Level: `L_MeridianHQ` with reception, analyst workspace, manager's office, meeting room, monitoring room, boardroom, training area.

## Determinism
All randomness flows from `GameState.Seed`. Organisation events re-derive each draw from `(EventSeed, EventDraws)`, so saving mid-scenario and resuming gives the same outcomes (`Save_ResumeContinuesSameRandomSequence`).

## Data
Mission 001 content is in C++ tables in `RbMission001.cpp` for now. Gate 4 moves mission content to Data Assets / validated data files under `Data/` with a schema validator test.
