# Known issues (0.5.1-alpha)

Honest list for testers. Severity: **High** affects most players, **Medium** noticeable, **Low** cosmetic. Walkthrough findings are numbered as in Docs/WalkthroughReport.md.

| Area | Issue | Severity | Notes |
|---|---|---|---|
| Testing | The whole campaign has been played through the real UI by a scripted walkthrough (Docs/WalkthroughReport.md), but not yet by a person. Pacing, difficulty and feel are unconfirmed. | High | Owner playtest pending. |
| Release | The executable and installer are unsigned, so Windows SmartScreen warns "Unknown publisher". | Medium | Needs a code-signing certificate (Docs/Release.md "Signing"). |
| Audio | No music or sound effects yet. The volume setting works but has nothing to control. | Medium | Walkthrough #15. |
| Pacing | Incidents from weak decisions rarely happen: in the walkthrough four deliberately weak choices passed before one incident fired. A player experimenting with weak choices may never see a crisis. | Medium | Walkthrough #11. Earlier good controls mitigating incidents is by design. |
| Crises | A crisis opened by an incident (C004-C012) does not say which earlier decision caused it. | Medium | Walkthrough #12. |
| Missions | Missions after 001 happen in the Case files screen at the workstation, not as walk-around scenes with characters and props. | Medium | Briefings and evidence are text. |
| Characters | Priya Okafor uses the Unreal mannequin as a stand-in body. | Medium | MetaHuman/Fab content was not used (free-asset rule). |
| Input | Keyboard and mouse only; gamepads are not bound. | Medium | |
| Interaction | The meeting-room maintenance binder is hard to target: the E prompt may need a small turn of the view. | Low | Walkthrough #13. |
| Case files | The list grows to 33 entries; locked cases far above your level make it long to scroll. | Low | Walkthrough #14. |
| Career | There is no level-1 tutorial (M000); new games start at level 2, Junior Risk Analyst. | Low | The ladder lists M000 for level 1 only. |
| Content | Wrong answers are now written as plausible, reasoned choices and answer length no longer gives the best one away (test-guarded), but some may still be easy to rule out. | Low | Tuning from playtests. |
| UI | Notices show one at a time while a screen is open (the newest replaces older ones). | Low | By design, to avoid covering screen content. |
| Monitoring room | Part of the KRI status strip is hidden behind the operator desks from some angles. | Low | |
| Saves | The checksum detects accidental corruption, not deliberate editing. Edited saves load with values brought into range. | Low | By design (single-player, offline). |
| Language | English only. | Low | |
| Hardware | Tested on one PC (GeForce GTX 1650, 1920x1080). Lower-end GPUs untested; use the Low or Medium graphics setting if the frame rate is poor. | Low | |
