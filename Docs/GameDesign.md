# Game design

## Pillars
Real risk work, consequential decisions, competence over grinding, short resumable sessions, offline.

## Setting
Valtren Coldline Logistics, a fictional temperature-controlled logistics firm (other profiles: Orrinbrook Mutual Assurance, Quellard Grid Projects). HQ spaces: reception, junior analyst workspace, manager's office, meeting room, risk monitoring room, executive boardroom, training area.

## Fictional names
All companies, suppliers, banks, insurers, regulators-by-brand, products, logos and visible text in the game are invented. Before adding a name, web-search it and pick another if a real business uses it (or something confusingly close). Downloaded assets are inspected for real-world logos or brand text (monitors, laptops, packaging, signage, vehicles); any that show one are retextured or excluded, and the decision is noted in Docs/AssetCredits.md. Renamed in save v4: Valtren Coldline Logistics, Orrinbrook Mutual Assurance, Quellard Grid Projects, Kelvarin Refrigeration Services.

## Mission 001: The Unresolved Warning
Manager: Priya Okafor (Risk Manager). Site: Northgate cold store, Bay 3.
Hook: the third temperature excursion in six months spoiled 42,000 of client stock. Register entry R-014 still says Medium, controls Effective, last reviewed 14 months ago.

| # | Objective | Where | Sim call |
|---|---|---|---|
| 1 | Meet the manager | Manager's office | `MeetManager()` |
| 2 | Incident report | Analyst inbox | `ViewEvidence("incident_report")` |
| 3 | Register entry | Workstation | `ViewEvidence("risk_register_R014")` |
| 4 | Control evidence | Monitoring room terminal, meeting room binder, training clipboard | alarm / maintenance / manual logs |
| 5-10 | Missing info, inherent L/I, controls + residual, treatment, owner, rationale | Workstation assessment form | `Submit()` |
| 11 | Debrief | Manager's office | `Result` |
Also available: insurance schedule (manager's office), client contract (boardroom).

Submission requires 1-3; skipping other evidence is allowed but costs score. The rubric (`RbMission001.cpp`, weights: evidence 10, missing info 15, L/I 20, controls/residual 15, treatment 25, owner 5, rationale 10; pass >= 60 and not "accept") is never shown before submission.

Treatments: accept and monitor; service compressor + fix alarm escalation (recommended); replace plant (over-spend, above authority, escalated); increase insurance (exclusion makes it poor value).

## Consequences
- Every treatment has immediate cost effects and schedules the same later event: compressor failure in week 6 with base probability 0.55 (x0.8 Analyst, x1.2 Executive).
- If the player's fix is in place, probability x0.25 and severity x0.5.
- Accepting sets `m001_warning_ignored` for later missions to reference. Wrong owner costs stakeholder trust.
- Decision quality (rubric score) is recorded separately from outcome; the "Good Call, Bad Luck" achievement rewards understanding that.
- Campaign missions (Gate 3) use the same machinery from data: each option can change metrics, set flags and schedule consequences; an incident can open a crisis mission; later briefings mention earlier choices (Docs/CareerModel.md "Campaign progression").

## Difficulty
Analyst (hints on, incidents less likely), Professional, Executive (no hints by default, incidents more likely). Difficulty never changes the rubric.

## Achievements (50, data-driven)
`Data/Achievements.json` (Docs/MissionData.md "Achievements file"). The seven Mission 001 achievements keep their built-in rules (`RbGameState.cpp`); the rest cover career levels, missions and crises passed, quality of work, good decisions that stick (flags), outcomes over time (incidents avoided or mitigated, reputation, resilience), competence, retrying a failed mission and escalating above one's authority. All are verifiable from saved state. The journal shows "Achievements (n of 50)".

## Engagement rules
No streak penalties, no artificial scarcity, no timers on thinking, manual save plus checkpoint autosave.

## Campaign targets (Gate 4)
30+ missions, 12+ crises, 100+ decision points, 50+ achievements, 5 skill branches, 5+ rubrics, 10+ tools, 3 org profiles (data done), 3 difficulties (done), CRO capstone.

## Easter eggs
Three small credits to the creator, built from `RbCredits::CreatorName`, with no prompts and no effect on missions: a framed certificate in the training area, a book on the manager's shelf, and a brass inlay on the boardroom table. Local check: `-RbSelfTest=tour -RbTourEggs` (screenshots not published).
