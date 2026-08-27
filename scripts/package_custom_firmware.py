Import("env")

from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import subprocess

X4PRO_VERSION = "1.5.0-x4pro-playcompanion-r11"
X4_VERSION = "1.5.0-x4-playcompanion-r11"
X3_VERSION = "1.5.0-x3-playcompanion-r11"
APP_PARTITION_BYTES = 0x640000
WARN_PERCENT = 92.0
FAIL_PERCENT = 98.0


def _git(project_dir: Path, *args):
    try:
        return subprocess.check_output(
            ["git", *args], cwd=project_dir, text=True, stderr=subprocess.DEVNULL
        ).strip()
    except Exception:
        return "unknown"


def package_firmware(source, target, env):
    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))
    progname = env.subst("$PROGNAME")
    source_bin = build_dir / f"{progname}.bin"
    if not source_bin.exists():
        return

    pioenv = env.subst("$PIOENV")
    if not (pioenv.startswith("x4pro_r11") or pioenv.startswith("x4_r11") or pioenv.startswith("x3_r11")):
        raise RuntimeError(f"r11 packager refused unexpected environment: {pioenv}")

    is_x4pro = pioenv.startswith("x4pro_r11")
    is_x4 = pioenv.startswith("x4_r11")
    is_x3 = pioenv.startswith("x3_r11")
    version = X4PRO_VERSION if is_x4pro else (X4_VERSION if is_x4 else X3_VERSION)
    device_slug = "x4pro" if is_x4pro else ("x4" if is_x4 else "x3")
    target_label = "Xteink X4 Pro / ESP32-S3" if is_x4pro else ("Xteink X4 / ESP32-C3" if is_x4 else "Xteink X3 / ESP32-C3")

    # ESP images start with 0xE9. This is a cheap sanity check that catches a
    # wrong/non-firmware output before it gets copied into dist/.
    with source_bin.open("rb") as fh:
        if fh.read(1) != b"\xE9":
            raise RuntimeError("packager refused output: missing ESP image magic byte 0xE9")

    suffix = "-debug" if pioenv.endswith("_debug") else ""
    output_dir = project_dir / "dist"
    output_dir.mkdir(exist_ok=True)
    output_bin = output_dir / f"{device_slug}-playcompanion-r11{suffix}.bin"
    shutil.copy2(source_bin, output_bin)

    size = output_bin.stat().st_size
    percent = size / APP_PARTITION_BYTES * 100.0
    headroom = APP_PARTITION_BYTES - size
    if percent >= FAIL_PERCENT:
        raise RuntimeError(
            f"firmware image uses {percent:.1f}% of the OTA app partition; "
            f"r11 safety limit is {FAIL_PERCENT:.1f}%"
        )
    if percent >= WARN_PERCENT:
        print(f"WARNING: firmware image uses {percent:.1f}% of the app partition")

    digest = hashlib.sha256(output_bin.read_bytes()).hexdigest()
    (output_dir / f"{output_bin.name}.sha256").write_text(
        f"{digest}  {output_bin.name}\n", encoding="utf-8"
    )

    # Copy the generated Companion contact sheet next to each release when it
    # exists, so artwork can be reviewed without hunting through source files.
    preview = project_dir / "build_artifacts" / "companion-preview.png"
    if preview.exists():
        shutil.copy2(preview, output_dir / "companion-preview.png")

    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    manifest = {
        "firmware": output_bin.name,
        "version": version + ("-debug" if suffix else ""),
        "platformio_environment": pioenv,
        "target": target_label,
        "sha256": digest,
        "image_bytes": size,
        "app_partition_bytes": APP_PARTITION_BYTES,
        "image_partition_percent": round(percent, 2),
        "partition_headroom_bytes": headroom,
        "built_utc": now,
        "git_branch": _git(project_dir, "rev-parse", "--abbrev-ref", "HEAD"),
        "git_commit": _git(project_dir, "rev-parse", "--short", "HEAD"),
        "companions": ["Sophocles", "Vellum", "Octavo", "Noodle", "Lincoln"],
        "games": ["Knucklebones", "Murdle", "Minesweeper", "Solitaire", "D&Diagrams", "Connections"],
    }
    (output_dir / f"{device_slug}-release-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    (output_dir / f"{device_slug}-RELEASE.txt").write_text(
        "\n".join([
            f"Play+Companion r11 — {'X4 Pro' if is_x4pro else ('X4' if is_x4 else 'X3')}",
            f"Firmware: {output_bin.name}",
            f"Version: {manifest['version']}",
            f"Built (UTC): {now}",
            f"SHA-256: {digest}",
            f"Image size: {size:,} bytes ({percent:.1f}% of 0x640000 app partition)",
            f"Headroom: {headroom:,} bytes",
            f"Git: {manifest['git_branch']} {manifest['git_commit']}",
            "",
            "Companions: Sophocles, Vellum, Octavo, Noodle, Lincoln",
            "Games: Knucklebones, Murdle, Minesweeper, Solitaire, D&Diagrams, Connections",
            "Apps shelf: removed",
            "",
        ]), encoding="utf-8"
    )

    print(f"Packaged custom firmware: {output_bin}")
    print(f"SHA-256: {digest}")
    print(f"App partition headroom (binary): {headroom:,} bytes ({100-percent:.1f}%)")
    print(f"Release metadata: {output_dir / f'{device_slug}-release-manifest.json'}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)
