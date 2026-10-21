RISKBOUND {VERSION} - portable Windows build
=============================================

A first-person, offline, single-player career simulator about risk management.
Start as a Junior Risk Analyst at Valtren Coldline Logistics (a fictional company)
and work toward Chief Risk Officer: 30 missions, 12 crises and a CRO capstone.

RUN
  1. Unzip the whole folder anywhere you can write to (for example Documents\Games).
     Do not run it from inside the zip.
  2. Double-click Riskbound.exe.
  Windows may warn that the publisher is unknown: see UNSIGNED-BUILD.txt.

REQUIREMENTS
  Windows 10 or 11, 64-bit. A DirectX 12 graphics card (tested on a GeForce GTX 1650:
  60-80 fps at 1920x1080 on the default High setting). About 1 GB of disk space.
  If the game reports missing runtime files, install the Microsoft Visual C++
  Redistributable (x64) from Microsoft's website.

CONTROLS
  WASD / arrow keys move, mouse looks, E interacts, J journal, Esc pause.
  Menus work with the keyboard (Tab/arrows, Enter/Space, Esc back).
  Settings: text size, reduced motion, hints, volume, mouse sensitivity, graphics quality.

SAVES
  Saved automatically at checkpoints, locally only (no accounts, no network):
    %LOCALAPPDATA%\Riskbound\Saved\SaveGames\slot1.rbsave
  A backup (slot1.rbsave.bak) is kept. If the game reports a recovered save, it
  loaded the backup. To restore by hand: close the game and rename the .bak file.

UNINSTALL
  Delete the folder. To remove saves too, delete %LOCALAPPDATA%\Riskbound.

CREDITS AND LICENCES
  Created by M A Almahlaki - m.awwadh.k@gmail.com
  LinkedIn: https://www.linkedin.com/in/mohammed-almahlaki001/
  Built with Unreal Engine (see Licences\UnrealEngine-Notice.txt).
  Third-party assets (CC0) and the Inter typeface (SIL OFL): Licences folder.
  Riskbound's own code: MIT licence (Licences\Riskbound-code-MIT.txt).
  Development assistance: Claude Code by Anthropic.
  The same credits are in the game: main menu or pause menu > Credits.
