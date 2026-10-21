# Writing your own cases (mods and the scenario editor)

Teachers, trainers and players can add cases without touching the game's code.

1. Create a skeleton: `python Scripts/NewMission.py X001 "My first case"` writes `X001.json` in the current folder. Use your own id prefix (letters and digits, such as `X` or your initials) so it never clashes with the shipped cases.
2. Edit it. The format is Docs/MissionData.md: briefing, evidence, questions (single, multi, numeric, rating), optional consequences, relationship effects, located evidence and a board presentation.
3. Check it: `python Scripts/NewMission.py --check X001.json` runs the same strict checks the game uses (through the core test runner when it has been built; otherwise a quick structural check).
4. Install it: copy the file to the game's `Saved/Mods/Missions/` folder
   - installed game: `%LOCALAPPDATA%\Riskbound\Saved\Mods\Missions\`
   - editor or Development build: `<project>\Saved\Mods\Missions\`
5. Start the game. **Challenges and classroom** shows how many player-made cases loaded, or the first problem if they did not. Mods are added only if the whole catalogue still validates, so a broken mod never breaks the game.

Rules the validator enforces: weights add up to 100, every single-choice question has an answer scoring 80 or more, numeric questions have an explanation and a sensible tolerance, prerequisites exist, ids are unique. Keep every business name fictional.

Classroom use: put a mod case on each trainee's PC, use **Classroom (hot seat)** to have several people take it on one PC, and export the results as CSV.
