Import("env")
from pathlib import Path
import hashlib, json, shutil

def package_firmware(source, target, env):
    project = Path(env.subst("$PROJECT_DIR"))
    build = Path(env.subst("$BUILD_DIR"))
    src = build / "firmware.bin"
    if not src.exists():
        print("r11.4 packager: firmware.bin not found")
        return
    name = env.subst("$PIOENV")
    device = "x4pro" if name.startswith("x4pro") else ("x3" if name.startswith("x3") else "x4")
    outdir = project / "dist"
    outdir.mkdir(exist_ok=True)
    dst = outdir / f"{device}-playcompanion-r11.4.bin"
    shutil.copy2(src, dst)
    digest = hashlib.sha256(dst.read_bytes()).hexdigest()
    print(f"Packaged custom firmware: {dst}")
    print(f"SHA-256: {digest}")
    manifest = {"environment": name, "device": device, "release": "r11.4", "sha256": digest, "size": dst.stat().st_size}
    mf = outdir / f"{device}-release-manifest.json"
    mf.write_text(json.dumps(manifest, indent=2))
    print(f"Release metadata: {mf}")

env.AddPostAction("$BUILD_DIR/firmware.bin", package_firmware)
