# Building from source

## Requirements

- Windows 10/11 x64
- WSL 2 with the Ubuntu-24.04 distribution
- Git and 7-Zip's 7z.exe on Windows PATH
- Python 3, GNU Make, GCC, and MinGW-w64 in WSL; tools/setup.ps1 installs MinGW-w64 if it is missing

## Build and launch

From the repository root in PowerShell:

    .\tools\setup.ps1
    .\tools\build-core.ps1
    .\Start-RetroPvP.cmd

Setup downloads the pinned RetroArch 1.22.2 Windows runtime into ignored build/runtime/ and uses WSL for the cross-builds. Set RETROPVP_RETROARCH_DIR or pass -RetroArchRuntimePath to use a runtime in another location.

The first launch builds the patched frontend if needed. The core must be built once with tools/build-core.ps1. When prompted, select a local ROM matching the profile in config/games/smb1.json.

Useful rebuild commands:

    .\tools\build-retropvp-frontend.ps1
    .\tools\build-core.ps1
    .\tools\run-headless-goomba-check.ps1 -RomPath "C:\path\to\your\smb1.nes" -Mode enemy-fixtures
    .\tools\run-headless-goomba-check.ps1 -RomPath "C:\path\to\your\smb1.nes" -Mode bill-state

The headless checks run without opening the emulator. They still require the locally built core and a matching local ROM.

## Local tester package

tools/package-retropvp.ps1 creates a Windows tester ZIP from the local build and the ROM you select or pass with -RomPath. The ROM is copied into that local ZIP. The script does not upload it. Do not commit the ZIP or distribute it unless you have the rights to distribute the included game data.

Build output, upstream source checkouts, RetroArch user settings, saves, and states are local-only and ignored by Git.
