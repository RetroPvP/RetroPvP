#!/usr/bin/env bash
set -Eeuo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$project_root/build/retroarch-src"
output_dir="$project_root/build/retropvp-frontend"
build_log="$project_root/build/retropvp-frontend-build.log"
revision_file="$project_root/config/retroarch-revision.txt"
revision="$(sed -n 's/^commit=//p' "$revision_file")"
repo_url="$(sed -n 's/^url=//p' "$revision_file")"
patch_file="$project_root/patches/retroarch/001-retropvp-windows-menu.patch"
applied_patch="$project_root/build/.retropvp-applied-frontend.patch"
flags_file="$project_root/build/.retropvp-frontend-flags.sha256"
make_flags=(
  HOST_PREFIX=x86_64-w64-mingw32-
  HAVE_CG=0
  HAVE_GLSL=1
  HAVE_FREETYPE=0
  HAVE_OVERLAY=1
  HAVE_BUILTINZLIB=1
  HAVE_ZLIB=0
  "LDFLAGS=-L. -static-libgcc -static-libstdc++ -lxaudio2_8 -limm32"
)
flags_fingerprint="$(printf '%s\n' "${make_flags[@]}" | sha256sum | awk '{print $1}')"

if [[ -z "$revision" || -z "$repo_url" || ! -f "$patch_file" ]]; then
  echo "Invalid RetroArch build inputs." >&2
  exit 2
fi

if [[ ! -d "$source_dir/.git" ]]; then
  mkdir -p "$(dirname "$source_dir")"
  git clone --no-checkout "$repo_url" "$source_dir"
fi

if ! git -C "$source_dir" cat-file -e "$revision^{commit}" 2>/dev/null; then
  git -C "$source_dir" fetch --depth 1 origin "$revision"
fi

revision_changed=0
if [[ "$(git -C "$source_dir" rev-parse HEAD)" != "$revision" ]]; then
  if [[ -n "$(git -C "$source_dir" status --porcelain)" ]]; then
    echo "RetroArch source has local changes; refusing to switch revisions." >&2
    exit 3
  fi
  git -C "$source_dir" checkout --detach "$revision"
  revision_changed=1
fi

patch_changed=0
if git -C "$source_dir" apply --ignore-space-change --reverse --check "$patch_file" >/dev/null 2>&1; then
  # The current patch is already present, even if the marker predates it.
  cp "$patch_file" "$applied_patch"
else
  if [[ -f "$applied_patch" ]]; then
    git -C "$source_dir" apply --ignore-space-change --reverse --check "$applied_patch"
    git -C "$source_dir" apply --ignore-space-change --reverse "$applied_patch"
  fi
  git -C "$source_dir" apply --ignore-space-change --check "$patch_file"
  git -C "$source_dir" apply --ignore-space-change "$patch_file"
  cp "$patch_file" "$applied_patch"
  patch_changed=1
fi

flags_changed=1
if [[ -f "$flags_file" ]] && [[ "$(<"$flags_file")" == "$flags_fingerprint" ]]; then
  flags_changed=0
fi

mkdir -p "$output_dir"
if [[ "$revision_changed" == "1" || "$patch_changed" == "1" || "$flags_changed" == "1" ]]; then
  make -C "$source_dir" -f Makefile.win clean > "$build_log" 2>&1
else
  : > "$build_log"
fi

if ! make -C "$source_dir" -f Makefile.win -j8 "${make_flags[@]}" >> "$build_log" 2>&1; then
  tail -n 100 "$build_log" >&2
  exit 4
fi

if [[ ! -s "$source_dir/retroarch.exe" ]]; then
  echo "Build finished without producing $source_dir/retroarch.exe" >&2
  exit 5
fi

cp "$source_dir/retroarch.exe" "$output_dir/retroarch.exe"
printf '%s\n' "$flags_fingerprint" > "$flags_file"
file "$output_dir/retroarch.exe"
"$source_dir/retroarch.exe" --version
echo "Build log: $build_log"
echo "Built: $output_dir/retroarch.exe"
