#!/usr/bin/env bash
set -Eeuo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$project_root/.third_party/MesenCE"
build_dir="$project_root/build"
build_log="$build_dir/mesence-build.log"
revision_file="$project_root/config/mesence-revision.txt"
revision="$(sed -n 's/^commit=//p' "$revision_file")"
repo_url="$(sed -n 's/^url=//p' "$revision_file")"

if [[ -z "$revision" || -z "$repo_url" ]]; then
  echo "Invalid MesenCE revision file: $revision_file" >&2
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
  git -C "$source_dir" checkout --detach "$revision"
  revision_changed=1
fi

patch_file="$project_root/patches/mesence-smb1-director.patch"
applied_patch="$source_dir/.bulletbill-applied.patch"
patch_changed=0
if [[ -f "$patch_file" ]]; then
  if [[ -f "$applied_patch" ]]; then
    if ! cmp -s "$patch_file" "$applied_patch"; then
      git -C "$source_dir" apply --reverse --check "$applied_patch"
      git -C "$source_dir" apply --reverse "$applied_patch"
      rm "$applied_patch"
      git -C "$source_dir" apply --check "$patch_file"
      git -C "$source_dir" apply "$patch_file"
      cp "$patch_file" "$applied_patch"
      patch_changed=1
    fi
  elif git -C "$source_dir" apply --reverse --check "$patch_file" >/dev/null 2>&1; then
    cp "$patch_file" "$applied_patch" # Already applied outside this script.
  else
    git -C "$source_dir" apply --check "$patch_file"
    git -C "$source_dir" apply "$patch_file"
    cp "$patch_file" "$applied_patch"
    patch_changed=1
  fi
elif [[ -f "$applied_patch" ]]; then
  git -C "$source_dir" apply --reverse --check "$applied_patch"
  git -C "$source_dir" apply --reverse "$applied_patch"
  rm "$applied_patch"
  patch_changed=1
fi

mkdir -p "$build_dir"
input_fingerprint_file="$build_dir/.director-inputs.sha256"
input_fingerprint="$(
  {
    # Makefile.libretro lacks generated header dependencies. Headers and generated
    # profile inputs therefore force a clean rebuild; copied .cpp files rebuild
    # their own objects incrementally through make.
    find "$project_root/src/director" -type f -name '*.h' -print0 | sort -z | xargs -0 sha256sum
    sha256sum "$project_root/config/games/smb1.json" "$project_root/tools/generate_smb1_profile.py"
    if [[ -f "$patch_file" ]]; then
      sha256sum "$patch_file"
    else
      printf 'no-director-patch\n'
    fi
  } | sha256sum | awk '{print $1}'
)"
director_inputs_changed=1
if [[ -f "$input_fingerprint_file" ]] && [[ "$(<"$input_fingerprint_file")" == "$input_fingerprint" ]]; then
  director_inputs_changed=0
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required to compile the SMB1 game profile." >&2
  exit 5
fi
python3 "$project_root/tools/generate_smb1_profile.py" \
  "$project_root/config/games/smb1.json" \
  "$build_dir/generated/Smb1ProfileData.h"
mkdir -p "$source_dir/BulletBill"
cp "$project_root"/src/director/*.h "$source_dir/BulletBill/"
cp "$project_root"/src/director/*.cpp "$source_dir/BulletBill/"
cp "$build_dir/generated/Smb1ProfileData.h" "$source_dir/BulletBill/Smb1ProfileData.h"

if [[ "$revision_changed" == "1" || "$patch_changed" == "1" || "$director_inputs_changed" == "1" || "${CLEAN_BUILD:-0}" == "1" ]]; then
  make -C "$source_dir" -f Makefile.libretro clean > "$build_log" 2>&1
else
  : > "$build_log"
fi

if ! make -C "$source_dir" -f Makefile.libretro -j2 \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ >> "$build_log" 2>&1; then
  tail -n 100 "$build_log" >&2
  exit 4
fi

core="$source_dir/mesen2_libretro.dll"
if [[ ! -s "$core" ]]; then
  echo "MesenCE build finished without producing $core" >&2
  exit 3
fi

cp "$core" "$build_dir/mesen2_libretro.dll"
printf '%s\n' "$input_fingerprint" > "$input_fingerprint_file"
file "$build_dir/mesen2_libretro.dll"
if [[ "$patch_changed" == "1" ]]; then
  echo "Applied director patch: $patch_file"
fi
echo "Build log: $build_log"
echo "Built: $build_dir/mesen2_libretro.dll"
