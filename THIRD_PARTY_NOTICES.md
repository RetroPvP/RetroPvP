# Third-party notices

## MesenCE

The emulator core is based on [MesenCE](https://github.com/libretro/MesenCE), pinned to the commit in `config/mesence-revision.txt`, and licensed under GPL-3.0. The project applies a tracked patch at build time. Release bundles include the core and the upstream license text in `licenses/MesenCE-LICENSE.txt`.

## RetroArch

The patched frontend is built from [RetroArch](https://github.com/libretro/RetroArch), pinned to `config/retroarch-revision.txt` and licensed under GPL-3.0. Release bundles include the frontend and upstream license text in `licenses/RetroArch-COPYING.txt`.

The required Ozone menu assets come from the official portable Windows archive. Their license text is included in a release bundle as `licenses/RetroArch-assets-COPYING.txt`. Upstream source and generated binaries remain outside the Git repository; the tracked patches and revision pins are the corresponding build inputs.
