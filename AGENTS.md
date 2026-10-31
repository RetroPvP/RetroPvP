# Repository guidance

RetroPvP is a Windows x64 proof of concept for one verified SMB1 ROM profile. Read the relevant page under `docs/` before changing build, runtime, controls, or Netplay behavior.

- Project-owned code and inputs live in `src/`, `config/`, `patches/`, and `tools/`. `build/`, `.third_party/`, `retroarch-userdata/`, and local ROM/save files are generated or private; never add them to Git.
- The MesenCE and RetroArch revisions are pinned in `config/`. Keep each source patch compatible with its pin and update the pin and patch together.
- `config/games/smb1.json` is the source of truth for ROM identity and game addresses. Do not duplicate those values in tools or adapter code.
- The ROM is not part of this project. Do not commit, upload, or include it in a public release. Validate the supplied local image by its profile hash.
- Keep behavior claims in docs tied to source inspection or an observed check. Label untested Netplay, rollback, and game coverage as unverified.
- Use the focused build or runtime check relevant to the change. Avoid broad test frameworks and repeated full builds when a smaller check gives the needed evidence.
- Preserve local user configuration and emulator processes. Never print credentials or copy secrets into the repository.
