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

## Living world (phase 1 of the 30-idea roadmap, Docs/Roadmap.md)
Rules in `RbWorld` (RiskboundSim), content in Data/People.json, Story.json and Events.json (Docs/WorldData.md). Everything is deterministic and saved (save v6).
- **Weak decisions leave fallout.** A single-choice answer scoring below 50 (with no consequence of its own) schedules a latent incident 2-6 weeks later with probability 0.6 x (50 - score) / 50 (x difficulty); a wrong pick in a multi-select, 0.2. Text and effects come from the mission department's fallout template. Good answers leave nothing behind. When an incident happens, the inbox names the decision it came from.
- **Crises name their cause and can escalate.** An incident-triggered crisis briefing ends with the decision that led to it. Those crises (C004-C012) must be picked up within 6 weeks of opening; left longer, they escalate without the player (reputation, trust, cash, exposure), close, and the news reports it. Crises the career ladder needs never escalate (validated).
- **People remember.** Fifteen characters (Data/People.json) hold a relationship from -100 to 100 (Hostile, Wary, Neutral, Friendly, Ally). Mission choices that the named people care about move it (+5 for an answer scoring 80+, -4 below 40, plus hand-written effects); inbox replies move it more. Relationships change briefings, open events and the story, and never gate promotion: promotion stays competence evidence and passed missions (Docs/CareerModel.md).
- **Office politics.** Executives pull two ways (growth: COO, CEO, Commercial; prudence: Finance, Compliance, the Risk Committee chair). Backing one side costs the other; a rival analyst (Callum Reyes) competes for credit and can become an ally.
- **The inbox interrupts.** Events arrive as time passes (at most three per update): colleagues, the regulator, home, the newswire. Replies with a deadline (1-3 weeks, i.e. one or two missions) apply their default outcome if the player lets them lapse. No timers on thinking: deadlines are in organisation weeks, which pass only when the player submits work.
- **Stress and home.** Stress (0-100, start 20) rises with each mission (+4), crisis (+12) and capstone (+8) and with some choices, and falls 3 a week. At 90 the player is sent home for two weeks: time passes without them. Stress never changes a score. Sam, the player's partner, writes about dinners, weekends and worry; at 70+ the HUD edge reddens and the music turns tense.
- **Regulator.** The Cold Chain Safety Authority's supervisor (Imogen Raske) inspects when controls are weak, requests data after M025 and can fine Valtren; cooperation builds a relationship that eases supervision.
- **Story: The Corvane Ledger.** A procurement fraud runs through the career: duplicate Corvane invoices, emergency-override approvals, a standby contract with a shell company (Tessaline Haulage Services) owned by the COO's brother, a 15% rebate side letter, speak-up reports closed by the COO's office, and finally a bribe of sponsorship. Ten clues come from optional evidence in missions and from inbox replies; four optional chapters (S001-S004, department "Investigation") open as clues accumulate and stay hidden until then. The final choice sets `story_exposed`, `story_buried` or `story_leaked`, which the CRO capstone reflects (endings are expanded in phase 4).
- **Visible consequences.** A share index (RbQuant `ShareIndex`, Docs/RiskMethodology.md section 15) is shown on the reception board and the HUD; headlines from incidents and events scroll in the monitoring room and appear in the journal.
- **Sound.** Adaptive music (menu, focus, tension when a crisis is open, an incident is unread or stress is high, crisis during a crisis or the capstone) with crossfades, positional ambience (servers, air handling, typing), UI and event cues, footsteps (`URbAudioSubsystem`, Docs/AssetCredits.md "Audio").

## In the world, with real tools (phase 2)
- **Tool-scored questions.** Missions mix option questions with typed figures and matrix ratings, scored by how close the player gets (Docs/MissionData.md). Eight earlier figure questions were converted. The workstation tools (loss scenarios, cost-benefit, Monte Carlo) are where the figures are worked out.
- **Casework in person.** Interviews and documents are placed in the office for the active case (spokespeople at a standing spot, a case folder on a surface, a small teal tag); the case file says where to go and the objective line points there. Board papers (M020, M023, the CRO capstone, The Ledger) are drafted at the workstation and presented at the boardroom lectern; drafted answers carry over for the session.
- **Case studies.** M031-M036 are optional missions inspired by real events (a canal blockage, a winter grid failure, a client concentration collapse, a rate shock on "safe" bonds, ransomware at a fuel supplier, a credit crunch), with fictional names and figure and rating questions.
- **Living office.** Each organisation week lands at a different time of day (morning, midday, afternoon, evening) and some weeks are overcast; the light changes slowly. Staff in the office follow employee morale (3 to 9 people). The player's desk carries a brass nameplate with their title and a trophy for every five achievements; the Chief Risk Officer gets a plaque at the head of the boardroom table.

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
