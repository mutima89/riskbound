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

## Build the game (Windows)
1. Install Unreal Engine 5.8 and Visual Studio 2022 (or Build Tools) with the C++ workload.
2. Compile the editor: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RiskboundEditor Win64 Development -project="%CD%\Riskbound.uproject"`
3. Automation tests: `Scripts\RunUnrealTests.bat "C:\Program Files\Epic Games\UE_5.8"`
4. Package: `Scripts\BuildWindows.bat "C:\Program Files\Epic Games\UE_5.8" Development` (output in `Release\Development\Windows\`).

Controls: WASD/arrows move, mouse looks, E interacts, J journal, Esc pause. Menus are keyboard navigable (Tab/arrows, Enter/Space, Esc back).

## Saves
Local only, no accounts or network. Backups and recovery: `Docs/SaveFormat.md`.

## License
MIT, see `LICENSE`.
