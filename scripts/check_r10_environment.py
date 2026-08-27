"""Fail early if an r10 build is pointed at the wrong hardware/partition map."""
Import("env")  # noqa: F821
from pathlib import Path
pioenv = env.subst("$PIOENV")
flags = " ".join(str(x) for x in env.get("BUILD_FLAGS", []))
project = Path(env.subst("$PROJECT_DIR"))
partitions = (project / "partitions.csv").read_text(encoding="utf-8")
if "app0" not in partitions or "0x640000" not in partitions:
    raise RuntimeError("r10 safety check: expected OTA partition layout is missing")
if pioenv.startswith("x4pro_r10"):
    board = env.subst("$BOARD")
    if "FREEINK_DEVICE_X4PRO" not in flags:
        raise RuntimeError("r10 safety check: FREEINK_DEVICE_X4PRO is missing")
    if "esp32-s3" not in board.lower():
        raise RuntimeError(f"r10 safety check: X4 Pro must use ESP32-S3 board, got {board}")
    print(f"r10 target check OK: {pioenv} / {board} / X4 Pro ESP32-S3")
elif pioenv.startswith("x4_r10"):
    board = env.subst("$BOARD")
    if "FREEINK_DEVICE_X4=1" not in flags or "FREEINK_DEVICE_X4PRO" in flags:
        raise RuntimeError("r10 safety check: X4 build flags are incorrect")
    if "esp32-c3" not in board.lower():
        raise RuntimeError(f"r10 safety check: X4 must use ESP32-C3 board, got {board}")
    if "BOARD_HAS_PSRAM" in flags:
        raise RuntimeError("r10 safety check: original X4 must not enable X4 Pro PSRAM flags")
    print(f"r10 target check OK: {pioenv} / {board} / X4 ESP32-C3")
