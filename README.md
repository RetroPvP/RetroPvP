# RetroPvP

RetroPvP is a Windows x64 proof of concept for asymmetric *Super Mario Bros.* Netplay: Player 1 plays Mario while Player 2 takes over supported enemies or spawns and steers a Bullet Bill.

**Download:** [RetroPvP 0.1.0-alpha.1 for Windows x64](https://github.com/RetroPvP/RetroPvP/releases/tag/v0.1.0-alpha.1). The release contains no game ROM; choose your own matching copy when the launcher asks for it.

## Supported game

The current profile supports one specific headered SMB1 ROM image. The core checks its SHA-1 against [`config/games/smb1.json`](config/games/smb1.json) and leaves the director inactive for other images. No ROM is included. Each player must supply their own legally obtained copy; do not commit ROMs, save files, or tester packages containing a ROM.

## Build and run

Requirements: Windows 10/11 x64, WSL 2 with Ubuntu 24.04, Git, Python 3 in WSL, and a `7z` command-line executable. `tools/setup.ps1` downloads the pinned RetroArch runtime and installs MinGW-w64 in the WSL distribution if it is missing.

```powershell
.\tools\setup.ps1
.\tools\build-core.ps1
.\Start-RetroPvP.cmd
```

On first launch, the patched RetroArch frontend is built if needed. Select the local ROM when prompted. To build the frontend separately, run `tools/build-retropvp-frontend.ps1`. See [Building from source](docs/building.md) for paths, inputs, and packaging details.

## Controls and Netplay

The project uses a shared RetroArch Netplay session. Both players need the same RetroPvP build, core, and ROM. Direct hosting requires an inbound port mapping; relay hosting avoids that requirement but routes traffic through a relay. The RetroPvP menu contains host, join, relay region, player bindings, emulator hotkeys, and volume controls. See [Controls](docs/controls.md) and [Netplay](docs/netplay.md).

## Current scope

The SMB1 director supports Bullet Bill control and horizontal takeover profiles for Goombas, green and red Koopas, and Buzzy Beetles. Enemies with distinct jump or swim routines are not supported by the current takeover controls. This remains an experimental prototype: broad ROM support, rollback stress testing, and representative offsite latency testing are open work.

## Repository guide

- [`AGENTS.md`](AGENTS.md): concise guidance for coding agents and maintainers.
- [`docs/architecture.md`](docs/architecture.md): runtime and patch boundaries.
- [`docs/building.md`](docs/building.md): reproducible build steps and local-only packaging.
- [`docs/controls.md`](docs/controls.md): default bindings and menu actions.
- [`docs/netplay.md`](docs/netplay.md): connection modes and relay configuration.
- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md): upstream projects and licenses.

## License

RetroPvP is distributed under GPL-3.0. Upstream components retain their own notices; see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
