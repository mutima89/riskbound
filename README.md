# Riskbound

A first-person, offline, single-player career simulator about risk management, built in Unreal Engine 5 for Windows. Start as a Junior Risk Analyst at a fictional logistics company and work toward Chief Risk Officer by investigating real-feeling risks, using working professional tools and living with the consequences of your decisions.

**Status:** pre-alpha. The simulation core is implemented and unit tested; the Unreal gameplay layer and Windows package are not built yet. See `Docs/Progress.md`.

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

## Build the game (Windows, once Gate 1 lands)
1. Install Unreal Engine 5.4 or later and Visual Studio 2022 with "Game development with C++".
2. Right-click `Riskbound.uproject` > Generate Visual Studio project files (or Switch Unreal Engine version).
3. Open `Riskbound.sln`, build `Development Editor | Win64`.
4. Package: `Scripts\BuildWindows.bat "C:\Program Files\Epic Games\UE_5.8"` (output in `Release\`).

## Saves
Local only, no accounts or network. Backups and recovery: `Docs/SaveFormat.md`.

## License
MIT, see `LICENSE`.
