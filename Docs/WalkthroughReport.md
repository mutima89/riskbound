# Walkthrough report: Riskbound 0.5.x, Shipping build

Date: 2026-10-21. Branch `claude/walkthrough-fixes`. Requested by the owner: "do complete walkthrough test".

## How the walkthrough was done
A complete play-through of the **Shipping** build, windowed at 1920x1080, through the **real UI**:

- **Driver.** `-RbSelfTest=walkthrough` (phase 1) and `-RbSelfTest=walkthrough2` (phase 2, a new process). Code: `Source/Riskbound/Private/RbWalkthrough.cpp`. Runner: `Scripts/RunWalkthrough.ps1 -Exe Release\Shipping\Windows\Riskbound.exe`.
- **Mouse.** The driver never calls game rules. It finds widgets on screen by their text and sends real Slate mouse moves and clicks at their positions. Slate hit-tests these on the real widgets, the same as a player's clicks.
- **Keyboard.** It types into text boxes with key-character events. Key presses (W, E, Esc, J) go through Enhanced Input.
- **Movement.** It walks the 3D office holding W along waypoints through doorways. It turns and aims with raw mouse deltas, calibrated at the start. It presses E when the interaction prompt appears, nudging the view like a player when the prompt does not appear at once.
- **Safety.** Input is injected only while the game window is the active application. On losing focus it pauses, and stops after 60 s. No OS-level input was sent, so nothing could reach other windows.
- **Audit.** On every screen, every visible text block is checked for clipping: it needs more space than it has, measured twice 0.4 s apart and ignoring text scrolled out of a viewport.
- **Output.** Screenshots at each major step. The full logs are `%LOCALAPPDATA%\Riskbound\Saved\RbWalkthrough_1.txt` and `_2.txt`. A selection of screenshots is in `Docs/Screenshots/walkthrough/`.

Coverage:

- **Phase 1:**
  1. Main menu, then New game on Professional.
  2. Walk to Priya Okafor for the briefing.
  3. Walk to the workstation. Open the inbox document and the R-014 register entry.
  4. Open **every risk tool**:
     - add a risk through the register form (typed);
     - run the Monte Carlo simulation;
     - export the executive report.
  5. Walk to all five evidence props in five rooms.
  6. Fill the **assessment form field by field**, including typing the rationale, and submit.
  7. Walk back for the debrief.
  8. Case files: M002 onward, with deliberate variations:
     - **one weak decision per mission** until an incident opened a crisis;
     - **M003 failed on purpose and then retried**.
  9. Play on to Team Lead (level 5).
  10. Pause, then Settings: text size slider and reduced-motion toggle.
  11. **Save game, then Quit to desktop.**
- **Phase 2:**
  1. New process: main menu, then **Continue**.
  2. Check that career and settings are restored and applied.
  3. Walk to the workstation.
  4. Play every remaining case, including the ladder crises and the optional specialisms, through the **CRO capstone**.
  5. Open the journal.

## Results (final run, Shipping)
Final Shipping run: phase 1 DONE (228 s), phase 2 DONE (158 s), new game to CRO capstone; 1 Low finding left (finding 13). Core 75/75, Unreal 11/11, Shipping self-tests play/resume/tour PASS (79.7 fps). Installer 0.5.1-alpha built; install, play from installed folder, uninstall (saves kept) PASS. Screenshots: Docs/Screenshots/walkthrough/.

## Findings
Severity: **Blocker** stops progress, **High** spoils the game for most players, **Medium** noticeable, **Low** minor. Status as of this branch.

| # | Severity | Area | Finding | Status |
|---|---|---|---|---|
| 1 | Blocker | Campaign | A player who passed every case at their level with weak evidence (after failing M003 once and making a few weak choices) was **stuck at Risk Analyst**: promotion needed stronger competence evidence, and no case was left to play. | **Fixed**: when stuck, passed cases at the current level reopen as **reviews** (evidence only, no organisational consequences or time). Case files explains it. Tested by `Campaign_StuckPlayerCanReviewPassedCasesWithoutConsequences`; the walkthrough hit it and recovered (review of M004, promotion). |
| 2 | High | Difficulty | **The best answer was the longest option** in 97% of single-choice questions (a median 3.6 times longer than the others), so picking the longest answer won even with shuffled order. Wrong answers were often straw men ("Tell them everything is fine"). | **Fixed**: 634 wrong-answer labels rewritten as plausible, reasoned choices, and some best answers tightened. Now the longest in 38% of questions, median length ratio 1.09. Guarded by `Content_AnswerLengthDoesNotGiveAwayTheBest`. Mission 001's treatment descriptions balanced too. |
| 3 | Medium | Risk tools | The **Risk register tab stayed highlighted** whichever tool was open (keyboard focus stayed on it; there was no active-tab style). | **Fixed**: the open tool's tab has the accent fill; focus follows the user. |
| 4 | Medium | Risk tools | **Register table clipped its text** (Title, Category, Appetite: "Cold-store temperature exc", "Within ap"). | **Fixed**: wider columns, ellipsis plus a tooltip for anything longer, and the heatmap moved beside the add-risk form so the table has the full width. |
| 5 | Medium | Risk tools | Appetite breaches in the register were **never shown in red**: the code compared "Exceeds" with the value "Exceeds appetite". | **Fixed**. |
| 6 | Medium | Assessment form | The title **collided with the "0 / 7 sections complete" counter** ("excursion0 / 7"). | **Fixed**: the counter moved to the eyebrow line. |
| 7 | Low | Assessment form | The "To submit, answer section 1, 2..." hint was **green while incomplete**. | **Fixed**: muted until complete. |
| 8 | Low | Settings | **Fullscreen showed as on in a windowed game** (it showed the saved preference, not the window). | **Fixed**: shows the actual window mode. |
| 9 | Low | Case files / journal | At the top of the ladder the debrief said "Next promotion still needs: Already at the top of the ladder". | **Fixed**: "You are at the top of the career ladder." |
| 10 | Low | Risk tools | Key labels in Loss scenarios / Cost-benefit wrapped awkwardly ("Expected loss (average per / year)"). | **Fixed**: wider key column. |
| 11 | Medium | Pacing | Incidents from weak decisions rarely happen: four missions with a deliberately weak choice passed before one incident fired (regulator finding, which opened crisis C007). Several were avoided because an earlier good control mitigated them, which is by design, but a player experimenting with weak choices may never see a crisis. | **Open**: consider higher probabilities for unmitigated weak options, or a guaranteed consequence for the most negligent ones. |
| 12 | Medium | Crises | An incident-triggered crisis (C004-C012) does not say which earlier decision caused it. | **Open**: add a briefing line naming the decision and the week. |
| 13 | Low | Interaction | The meeting-room maintenance binder is hard to target: the prompt appeared only after nudging the view. | **Open**: enlarge its interaction collision or raise it slightly. |
| 14 | Low | Case files | The case list grows to 33 entries; locked cases far above the player's level make it long to scroll. | **Open**: group by level or collapse levels above the next one. |
| 15 | Low | Audio | No music or sound effects (known issue). | **Open** (Docs/KnownIssues.md). |
| 16 | Info | Main menu | The menu background shows the current office (corridor, polished floor, glazed rooms). | Confirmed, no change needed. |
| 17 | Info | Reading load | Cases need 254-627 words of reading (median 376, about 2 minutes); the longest is M002 (about 3 minutes). Evidence items are short (median 21 words; one over 70). No wall-of-text problem. | No change. |

## What worked well
- Every scripted step through the real UI succeeded: walking, interacting, every screen and button, typing, the form, submit, debrief, case files, all 42 data cases, pause, settings, save, quit and Continue.
- Save, quit to desktop and Continue in a new process restored level, campaign records, settings and the applied UI text scale.
- Consequences and context carried across missions as designed: the briefing notes changed with earlier choices; a weak decision led to a regulator finding that opened a crisis; good controls prevented later incidents.
- The capstone, 42 of 50 achievements and the journal all reflected the career correctly.

## Not covered
- Human judgement of feel: mouse sensitivity, motion comfort, readability at a distance. The driver's own timing is not a person's.
- Gamepad (not supported) and resolutions other than 1920x1080.
- Executive and Analyst difficulty (covered by the core replay tests, not by this UI walkthrough).
