#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: generate_smb1_profile.py <profile.json> <output.h>", file=sys.stderr)
        return 2

    profile_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    sha1 = profile.get("sha1", "").lower()
    if not re.fullmatch(r"[0-9a-f]{40}", sha1):
        raise ValueError("profile sha1 must contain 40 hexadecimal characters")

    memory = profile["memory"]
    bill = profile["bullet_bill"]
    enemy_control = profile["enemy_control"]
    horizontal_enemies = enemy_control.get("horizontal_enemies", [])
    if not horizontal_enemies:
        raise ValueError("profile must list at least one horizontal enemy")
    fields = {
        "RomSize": profile["rom_size"],
        "EnemyFlag": memory["enemy_flag"],
        "EnemyId": memory["enemy_id"],
        "EnemyState": memory["enemy_state"],
        "EnemyMovingDirection": memory["enemy_moving_direction"],
        "EnemyXSpeed": memory["enemy_x_speed"],
        "EnemyPage": memory["enemy_page"],
        "EnemyX": memory["enemy_x"],
        "EnemyYHigh": memory["enemy_y_high"],
        "EnemyY": memory["enemy_y"],
        "PlayerY": memory["player_y"],
        "EnemyBoundingBox": memory["enemy_bounding_box"],
        "ScreenLeftPage": memory["screen_left_page"],
        "ScreenLeftX": memory["screen_left_x"],
        "ScreenRightPage": memory["screen_right_page"],
        "ScreenRightX": memory["screen_right_x"],
        "OperationMode": memory["operation_mode"],
        "GameModeValue": memory["game_mode_value"],
        "PauseStatus": memory["pause_status"],
        "DirectorState": memory["director_state"],
        "CannonBulletBillId": bill["cannon_variant_id"],
        "FrenzyBulletBillId": bill["frenzy_variant_id"],
        "EnemySlots": bill["slots"],
        "ControllableSlots": bill["controllable_slots"],
        "BoundingBoxValue": bill["bounding_box_value"],
        "SpawnInset": bill["spawn_inset"],
        "HeightMin": bill["height_min"],
        "HeightMax": bill["height_max"],
        "HeightStepFrames": bill["height_step_frames"],
        "SpeedSlow": bill["speed_slow"],
        "SpeedDefault": bill["speed_default"],
        "SpeedFast": bill["speed_fast"],
        "LeftDirection": bill["left_direction"],
        "VerticalHighPosition": bill["vertical_high_position"],
        "DefeatedStateMask": bill["defeated_state_mask"],
        "RightDirection": enemy_control.get("right_direction", 1),
    }
    constants = "\n".join(
        f"inline constexpr unsigned {name} = 0x{value:04X};"
        for name, value in fields.items()
    )
    enemy_rows = "\n".join(
        f'\t{{ 0x{enemy["id"]:02X}, "{enemy["label"]}" }},'
        for enemy in horizontal_enemies
    )
    output = (
        "#pragma once\n"
        "namespace RetroPvP::Smb1ProfileData {\n"
        f'inline constexpr char Sha1[] = "{sha1}";\n'
        f"{constants}\n"
        "struct HorizontalEnemyProfile { unsigned id; const char* label; };\n"
        "inline constexpr HorizontalEnemyProfile HorizontalEnemies[] = {\n"
        f"{enemy_rows}\n"
        "};\n"
        "inline constexpr unsigned HorizontalEnemyCount = "
        f"{len(horizontal_enemies)};\n"
        "}\n"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(output, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
