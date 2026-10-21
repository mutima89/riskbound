# Riskbound

A first-person, offline, single-player career simulator about risk management, built in Unreal Engine 5 for Windows. Start as a Junior Risk Analyst at a fictional logistics company and work toward Chief Risk Officer by investigating real-feeling risks, using working professional tools and living with the consequences of your decisions.

**Status:** alpha. The full campaign is playable: 30 missions, 12 crises and a Chief Risk Officer capstone, nine working risk tools, 50 achievements. Windows Development and Shipping packages build and pass automated self-tests; hands-on playtesting is in progress. See `Docs/Progress.md`.

## Repository
- `Source/RiskboundSim/` - simulation rules (plain C++17, testable without Unreal)
- `Source/Riskbound/` - Unreal gameplay module
- `Tests/Core/` - simulation unit tests
- `Docs/` - design, methodology, career model, save format, testing, progress
- `Scripts/` - test, build and packaging scripts
- `.claude/skills/` - Claude Code project skills for recurring development tasks

## Run the simulation tests
Linux/macOS: `Scripts/run_core_tests.sh`
Windows (CMake + Visual Studio C++): `Scripts\RunCoreTests.bat`

## Build the game (Windows)
1. Install Unreal Engine 5.8 and Visual Studio 2022 (or Build Tools) with the C++ workload.
2. Compile the editor: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RiskboundEditor Win64 Development -project="%CD%\Riskbound.uproject"`
3. Automation tests: `Scripts\RunUnrealTests.bat "C:\Program Files\Epic Games\UE_5.8"`
4. Package: `Scripts\BuildWindows.bat "C:\Program Files\Epic Games\UE_5.8" Development` (or `Shipping`; output in `Release\<Config>\Windows\`).
5. Installer and portable zip: `Scripts\BuildInstaller.bat 0.5.0-alpha 0.5.0.0` (needs Inno Setup 6; output in `Release\`). Release checklist: `Docs/Release.md`.

Controls: WASD/arrows move, mouse looks, E interacts, J journal, Esc pause. Menus are keyboard navigable (Tab/arrows, Enter/Space, Esc back).

## Saves
Local only, no accounts or network. Backups and recovery: `Docs/SaveFormat.md`.

## Credits
Created by M A Almahlaki - m.awwadh.k@gmail.com - LinkedIn: https://www.linkedin.com/in/mohammed-almahlaki001/

Built with Unreal Engine 5.8 (Unreal is a trademark of Epic Games, Inc.). Third-party assets (CC0 and SIL OFL) and their authors: Docs/AssetCredits.md. Development assistance: Claude Code by Anthropic. The same credits are in the game (main menu and pause menu > Credits).

## License
MIT, see `LICENSE`.
