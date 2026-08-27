"""PlatformIO pre-build script: ensure CrossPoint's JPEGDEC fixes are present.

r8 deliberately does *not* use ``git apply``. PlatformIO installs JPEGDEC into
.pio/libdeps and Windows occasionally locks jpeg.inl while Git tries to unlink
and replace it. That caused WinError 32/permission failures in otherwise-clean
builds.

This script performs two exact, idempotent source substitutions in-place. On a
normal incremental build it reads the marker and returns immediately. On a
fresh dependency install it patches once. No Git process, no unlink/rename, and
no repeated writes.
"""

Import("env")  # noqa: F821
from pathlib import Path
import time

MARKER_1 = "CrossPoint patch: redirect pMCU to sMCUs[0] when MCU_SKIP"
MARKER_2 = "CrossPoint patch: guard against MCU_SKIP"

OLD_PTR = "    signed short *pMCU = &pJPEG->sMCUs[iMCU & 0xffffff];"
NEW_PTR = """    // CrossPoint patch: redirect pMCU to sMCUs[0] when MCU_SKIP to avoid
    // a wild pointer (~33 MB past sMCUs) that store-faults on AC writes.
    signed short *pMCU = (iMCU < 0) ? pJPEG->sMCUs
                                    : &pJPEG->sMCUs[iMCU & 0xffffff];"""

OLD_OR = "                pMCU[0] |= iPositive;"
NEW_OR = """                // CrossPoint patch: guard against MCU_SKIP. The pMCU
                // redirect makes &sMCUs[0] safe to dereference, but
                // writing here would clobber the just-decoded Y DC.
                if (iMCU >= 0)
                    pMCU[0] |= iPositive;"""

OLD_DC = "        pMCU[0] = (short)*iDCPredictor; // store in MCU[0]"
NEW_DC = """        // CrossPoint patch: guard against MCU_SKIP. See note on the
        // matching SA write above.
        if (iMCU >= 0)
            pMCU[0] = (short)*iDCPredictor; // store in MCU[0]"""


def _write_with_retry(path: Path, text: str) -> None:
    last = None
    for attempt in range(6):
        try:
            # Write directly rather than replacing the file. This avoids the
            # Windows unlink operation that made the old git-apply path brittle.
            path.write_text(text, encoding="utf-8")
            return
        except PermissionError as exc:
            last = exc
            time.sleep(0.5 * (attempt + 1))
    raise RuntimeError(
        f"JPEGDEC source is locked by another process: {path}. "
        "Close duplicate VS Code/PlatformIO builds and retry."
    ) from last


def ensure_patched(jpeg_inl: Path) -> None:
    text = jpeg_inl.read_text(encoding="utf-8")
    if MARKER_1 in text and MARKER_2 in text:
        return

    original = text
    if MARKER_1 not in text:
        if OLD_PTR not in text:
            raise RuntimeError(
                f"JPEGDEC source at {jpeg_inl} no longer matches the pinned revision; "
                "refusing to guess at the progressive-JPEG safety patch."
            )
        text = text.replace(OLD_PTR, NEW_PTR, 1)

    if MARKER_2 not in text:
        if OLD_OR not in text or OLD_DC not in text:
            raise RuntimeError(
                f"JPEGDEC source at {jpeg_inl} no longer matches the pinned revision; "
                "refusing to guess at the DC-write safety patch."
            )
        text = text.replace(OLD_OR, NEW_OR, 1)
        text = text.replace(OLD_DC, NEW_DC, 1)

    if text != original:
        _write_with_retry(jpeg_inl, text)
        print(f"JPEGDEC safety fixes applied once: {jpeg_inl}")


project = Path(env.subst("$PROJECT_DIR"))
for jpeg_inl in project.glob(".pio/libdeps/*/JPEGDEC/src/jpeg.inl"):
    ensure_patched(jpeg_inl)
