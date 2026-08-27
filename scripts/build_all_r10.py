"""One-click PlatformIO wrapper: build both X4 and X4 Pro r10 in one PIO run."""
Import("env")  # noqa: F821
from pathlib import Path
import subprocess, sys
project = Path(env.subst("$PROJECT_DIR"))
print("\n" + "="*72)
print("R10 BUILD ALL: X4 + X4 Pro (single PlatformIO invocation)")
print("="*72)
cmd = [sys.executable, "-m", "platformio", "run", "-e", "x4_r10", "-e", "x4pro_r10"]
result = subprocess.run(cmd, cwd=project)
if result.returncode:
    raise SystemExit(result.returncode)
print("\nR10 BUILD ALL SUCCESS")
print(f"X4:     {project / 'dist' / 'x4-playcompanion-r10.bin'}")
print(f"X4 Pro: {project / 'dist' / 'x4pro-playcompanion-r10.bin'}")
raise SystemExit(0)
