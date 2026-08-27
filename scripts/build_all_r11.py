"""One-click r11 wrapper: build X3, X4, and X4 Pro concurrently."""
Import("env")  # noqa: F821
from pathlib import Path
import subprocess, sys, threading
project = Path(env.subst("$PROJECT_DIR"))
targets = ["x3_r11", "x4_r11", "x4pro_r11"]
print("\n" + "="*76)
print("R11 BUILD ALL: X3 + X4 + X4 Pro (PARALLEL)")
print("="*76)

procs = {}
logs = {}
for target in targets:
    log_path = project / f"build-{target}.log"
    fh = open(log_path, "w", encoding="utf-8", errors="replace")
    cmd = [sys.executable, "-m", "platformio", "run", "-e", target]
    procs[target] = (subprocess.Popen(cmd, cwd=project, stdout=fh, stderr=subprocess.STDOUT, text=True), fh, log_path)
    print(f"Started {target} -> {log_path.name}")

failed = []
for target in targets:
    proc, fh, log_path = procs[target]
    rc = proc.wait()
    fh.close()
    if rc:
        failed.append((target, rc, log_path))
    print(f"{target}: {'SUCCESS' if rc == 0 else 'FAILED'}")

if failed:
    print("\nR11 BUILD ALL FAILED")
    for target, rc, log_path in failed:
        print(f"  {target}: exit {rc}; see {log_path}")
        try:
            lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
            print("\n".join(lines[-35:]))
        except Exception:
            pass
    raise SystemExit(1)

print("\nR11 BUILD ALL SUCCESS")
print(f"X3:     {project / 'dist' / 'x3-playcompanion-r11.bin'}")
print(f"X4:     {project / 'dist' / 'x4-playcompanion-r11.bin'}")
print(f"X4 Pro: {project / 'dist' / 'x4pro-playcompanion-r11.bin'}")
raise SystemExit(0)
