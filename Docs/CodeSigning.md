# Code signing

Riskbound's installer and game are not signed yet, so Windows shows "Unknown publisher" and SmartScreen may warn on download. The owner chose (2026-10-22) to apply to **SignPath Foundation**, which signs open-source projects for free. Nothing is bought; no certificate or password is stored in the repository.

## What SignPath Foundation needs (check against their current terms before applying)
- An OSI-approved licence: Riskbound's code is MIT. Third-party content is credited in Docs/AssetCredits.md (CC0, CC BY 4.0, MIT, OFL).
- A public repository with the source of what is signed: github.com/mutima89/riskbound.
- Builds that SignPath can trace to that source, normally a CI build (GitHub Actions) whose artifact is submitted for signing.
- A short description, the project's website or README, and who is responsible for releases.
- Signed files must not contain anything that misleads users (no bundled adware, no hidden network use). Riskbound is offline; the only network use is the opt-in AI colleagues feature, off by default, with the player's own key (Docs/AiColleagues.md).

## The likely sticking point
Riskbound is built with Unreal Engine 5. The engine is free to use but not open source (Epic's EULA), and it cannot be installed on GitHub's hosted build machines in a practical way (tens of gigabytes, an Epic account). So:
1. SignPath may ask whether a project built on a proprietary engine qualifies. The game's own code and data are MIT, but say plainly in the application that it uses Unreal Engine.
2. The CI build would have to run on a **self-hosted runner** (the owner's PC with UE 5.8.3). Ask SignPath whether they accept artifacts from a self-hosted GitHub Actions runner for origin verification; if they only accept hosted runners, the Foundation route is closed and the options below apply.

## Application text (paste into the SignPath Foundation form)
> **Project:** Riskbound
> **Repository:** https://github.com/mutima89/riskbound (MIT licence)
> **Description:** An offline, single-player, first-person career simulator for learning risk management. The player rises from graduate trainee to Chief Risk Officer by doing real risk work: risk registers, expected-loss and Monte Carlo tools, interviews and board papers. Free, no accounts, no telemetry.
> **What would be signed:** the Windows installer (Inno Setup) and Riskbound.exe from each GitHub release.
> **Build:** Unreal Engine 5.8 (proprietary engine, free licence). The game code, scripts and data in the repository are MIT; third-party art, audio and fonts are CC0, CC BY 4.0, MIT or OFL and credited in Docs/AssetCredits.md. Because the engine cannot run on GitHub-hosted runners, builds would run on a self-hosted GitHub Actions runner owned by the maintainer. Is that acceptable for origin verification?
> **Network use:** none by default. An optional AI-colleague feature, off by default, calls an AI provider with the player's own API key.
> **Maintainer:** mutima89 (GitHub).

## If SignPath approves: the signing step
SignPath gives an organisation id, a project slug and a signing policy, and asks for an API token stored as a GitHub Actions secret (`SIGNPATH_API_TOKEN`; never in the repository). A release workflow on the self-hosted runner then:
1. builds Shipping with `Scripts\BuildWindows.bat` and the installer with `Scripts\BuildInstaller.bat`;
2. uploads the unsigned installer as a workflow artifact;
3. submits it with SignPath's GitHub action (`signpath/github-action-submit-signing-request`) and downloads the signed file;
4. attaches the signed installer to the GitHub release.
The workflow file is added only once SignPath has approved the project and given the ids, so the repository does not carry a workflow that cannot run.

## Other free routes, if SignPath declines
- **Microsoft Store (free for individual developers):** an MSIX package submitted to the Store is signed by Microsoft and installs without SmartScreen warnings. Needs a Partner Center account (no fee for individuals at the time of writing; check), MSIX packaging of the game, and Store certification.
- **Self-signed certificate:** free and immediate, but only removes the warning on PCs that manually trust the certificate. Not useful for other players.
- **Unsigned with instructions:** the current state. The README explains "More info, Run anyway".

Paid options (not used, owner rule: no purchases): standard or EV code-signing certificates, Azure Trusted Signing (monthly fee).
