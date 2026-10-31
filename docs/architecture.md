# Architecture

RetroPvP adds an SMB1-specific director to the existing MesenCE Libretro core and uses RetroArch for the Windows frontend and Netplay transport.

## Frame path

1. RetroArch polls the synchronized controller inputs for the current frame.
2. The MesenCE Libretro patch reads player 2's inputs and passes them to RetroPvP::BulletBillDirector.
3. The director is enabled only when the loaded ROM matches the profile SHA-1. config/games/smb1.json is the source of truth for the memory layout, enemy IDs, and control bounds.
4. The director updates supported SMB1 enemy state at the core's frame boundary. The game continues to render and collide with those native enemy objects.
5. The RetroArch frontend patch supplies the Windows RetroPvP menu and maps the client's second local controller to the synchronized second game port.

## Source and generated files

- src/director/ contains project-owned C++.
- patches/mesence-smb1-director.patch and patches/retroarch/ are the tracked integration points for the pinned upstream projects.
- tools/generate_smb1_profile.py generates profile constants from the JSON into ignored build output.
- Upstream checkouts, generated headers, executables, cores, and user data stay in ignored directories. Do not edit build/ or .third_party/ as source.

The director does not replace RetroArch's Netplay or invent a separate state synchronization protocol. Both peers must use the same pinned core and matching content.
