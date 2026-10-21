# Known issues (0.5.0-alpha)

Honest list for testers. Severity: **High** affects most players, **Medium** noticeable, **Low** cosmetic.

| Area | Issue | Severity | Notes |
|---|---|---|---|
| Testing | The campaign after Mission 001 has not been played through by a person yet. Automated tests prove every mission is reachable and winnable, not that the pacing or difficulty feel right. | High | Owner playtest pending. |
| Release | The executable and installer are unsigned, so Windows SmartScreen warns "Unknown publisher". | Medium | Needs a code-signing certificate (Docs/Release.md "Signing"). |
| Audio | No music or sound effects yet. The volume setting works but has nothing to control. | Medium | |
| Missions | Missions after 001 happen in the Case files screen at the workstation, not as walk-around scenes with characters and props. | Medium | Briefings and evidence are text. |
| Characters | Priya Okafor uses the Unreal mannequin as a stand-in body. | Medium | MetaHuman/Fab content was not used (free-asset rule). |
| Input | Keyboard and mouse only; gamepads are not bound. | Medium | |
| Career | There is no level-1 tutorial (M000); new games start at level 2, Junior Risk Analyst. | Low | The ladder lists M000 for level 1 only. |
| Content | Wrong answers are written to be plausible, but some may still be easy to rule out. | Low | Tuning from playtests. |
| UI | Notices show one at a time while a screen is open (the newest replaces older ones). | Low | By design, to avoid covering screen content. |
| Monitoring room | Part of the KRI status strip is hidden behind the operator desks from some angles. | Low | |
| Saves | The checksum detects accidental corruption, not deliberate editing. Edited saves load with values brought into range. | Low | By design (single-player, offline). |
| Language | English only. | Low | |
| Hardware | Tested on one PC (GeForce GTX 1650, 1920x1080). Lower-end GPUs untested; use the Low or Medium graphics setting if the frame rate is poor. | Low | |
